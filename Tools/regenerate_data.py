"""Regenerate the analytical bank and gain coefficients in Source/GeneratedData.h.

This extracts the active calibration formulas from the original Alpha V0.1
exporter and its Python prototype inputs. No samples, MIDI, or legacy imports
are needed. NumPy is required only by this optional development tool.
"""
import argparse
import json
from pathlib import Path

import numpy as np

SETTINGS = json.loads(Path(__file__).with_name("calibration_settings.json").read_text(encoding="utf-8"))
BASE_PARAMS = SETTINGS["base"]
BETA11 = SETTINGS["beta11"]
REGISTER = SETTINGS["register"]
CONTRAST = SETTINGS["contrast"]
ATTACK = SETTINGS["attack"]
FLUTTER = SETTINGS["flutter"]
RATE_RAMP = SETTINGS["rate_ramp"]
p = dict(BASE_PARAMS, **BETA11["parameter_overrides"])


def smoothstep(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def frequency(midi):
    return 440.0 * 2.0 ** ((midi - 69.0) / 12.0)


def controls(t, gate, velocity, expression, p):
    """Separate amplitude, breath pressure and articulation trajectories."""
    vel = velocity / 127.0
    attack = p['attack_s'] * (1.22 - 0.40 * vel)
    rise = smoothstep(t / attack)
    # Pressure peaks after tongue release; a stronger tongue is not a click.
    bloom = (1.0 - np.exp(-t / p['bloom_s']))
    accent = p['accent'] * np.exp(-((t - attack * 1.4) / p['accent_s']) ** 2)
    swell = expression * (0.5 - 0.5 * np.cos(2 * np.pi * np.minimum(t / gate, 1)))
    onset_floor = p.get('pressure_onset_floor', .66)
    pressure = (0.19 + 0.73 * vel ** 0.82) * (onset_floor + (1-onset_floor) * bloom)
    pressure += accent * vel + swell
    pressure += p['pressure_flutter'] * np.sin(2 * np.pi * 3.7 * t + .4)
    pressure = np.clip(pressure, .02, 1.15)
    body = (1 + 0.13 * vel * np.exp(-t / .10)) * (1 + 0.25 * swell)
    amp = rise * body * (0.20 + 0.80 * vel ** 1.35)
    # Release starts at note-off without discontinuity. Spectrum closes faster
    # than amplitude. Finite smooth taper reaches exactly zero at the tail.
    release = smoothstep(1 - (t - gate) / p['release_s'])
    amp *= release ** 1.65
    pressure *= .40 + .60 * release ** 2
    return amp, pressure, attack


def excitation_bank(max_harmonics=96):
    """Compute the formant excitation bank from analytical harmonics."""
    pressure_grid = np.linspace(0, 1.2, 33)
    bank = []
    for q in pressure_grid:
        # Analytic differentiated pulse family; NOT a stock saw oscillator.
        n = np.arange(1, max_harmonics + 1)
        magnitude = np.exp(-n / (5 + 33*q)) / n ** .72
        magnitude *= 1 + .20 * np.cos(np.pi * n)
        phase = -.28 * n + .12 * np.sin(n * .8)
        coeff = magnitude * np.exp(1j * phase)
        bank.append(coeff)
    bank = np.array(bank)
    # One common scale per bank, retaining pressure-dependent harmonic energy.
    bank /= np.max(np.sqrt(np.sum(abs(bank)**2, axis=1)))
    return pressure_grid, bank


grid, bank = excitation_bank()


def spectral_shape(hz, harmonic, pressure, t, gate, midi, p):
    """Moving spectral envelope, partial attacks and fixed/key-tracked formants.

    Applying this envelope to partial amplitudes is dynamic spectral filtering;
    no time-domain IIR or sampled instrument spectrum is needed.
    """
    key = 2 ** ((midi - 69) / 12 * p['formant_keytrack'])
    cutoff = p['cutoff_hz'] * (.24 + .99 * pressure ** p.get('spectral_pressure_power',1.45))
    cutoff *= 2 ** ((midi - 69) / 12 * p['cutoff_keytrack'])
    rolloff = 1 / np.sqrt(1 + (hz / np.maximum(cutoff, 100)) ** 6)
    formants = np.ones_like(pressure) * p['body_floor']
    for center, width_oct, gain in p['formants']:
        # Gentle movement avoids a static vowel/EQ signature.
        center_t = center * key * (1 + p['formant_motion'] * (pressure-.6))
        formants += gain * np.exp(-.5 * (np.log2(hz / center_t) / width_oct)**2)
    even_odd = 1 + p['odd_bias'] * (1 if harmonic % 2 else -1)
    fundamental = p['fundamental'] if harmonic == 1 else 1.0
    # Higher partials open slightly later; release closes their energy earlier.
    partial_attack = 1 - np.exp(-t / (p['partial_bloom_s'] * (1 + .10*harmonic)))
    release_t = np.maximum(t-gate, 0)
    partial_release = np.exp(-release_t * harmonic / p['spectral_release_scale'])
    return rolloff * formants * even_odd * fundamental * partial_attack * partial_release


def beta_controls(t, gate, velocity, expression, p):
    amp, pressure, attack = controls(t, gate, velocity, expression, p)
    mod = BETA11['sustain_modulation']
    # Fade modulation in after the attack. Freeze it at note-off so no new
    # pulsing is introduced into the original amplitude release curve.
    held_time = np.minimum(t, gate)
    ramp = smoothstep((held_time-mod['delay_s'])/mod['fade_in_s'])
    phase_time = np.maximum(held_time-mod['delay_s'],0)
    slow = np.sin(2*np.pi*mod['rate_hz']*phase_time)
    fine = np.sin(2*np.pi*mod['secondary_rate_hz']*phase_time)
    amp *= 1+ramp*(mod['amplitude_depth']*slow+mod['secondary_depth']*fine)
    release = smoothstep(1-(t-gate)/p['release_s'])
    pressure += mod['pressure_depth']*ramp*slow*(.40+.60*release**2)
    return amp, np.clip(pressure,.02,1.15), attack


def beta_spectral_shape(hz, harmonic, pressure, t, gate, midi, p):
    bright = spectral_shape(hz,harmonic,pressure,t,gate,midi,p)
    original = spectral_shape(hz,harmonic,pressure,t,gate,midi,BASE_PARAMS)
    # Keep the original darkening tail: most extra brightness returns to the
    # baseline shape during the first 25 ms after note-off, smoothly.
    tail = BETA11['release_brightness']
    weight = tail['remaining_fraction']+(1-tail['remaining_fraction'])*np.exp(
        -np.maximum(t-gate,0)/tail['return_s'])
    return original+(bright-original)*weight


def validate(value):
    if not np.isfinite(value) or not 0<=value<=100:raise ValueError('Parameter must be 0..100')
    return value/100


def register_gain(hz,h,midi,value):
    c=REGISTER['register'];s=smoothstep
    key=float(s((midi-c['start_midi'])/(c['full_midi']-c['start_midi'])))
    band=float(s((h-c['high_partial_start'])/(c['high_partial_full']-c['high_partial_start'])))
    band*=1-float(s((hz-c['taper_start_hz'])/(c['taper_end_hz']-c['taper_start_hz'])))
    return 10**(c['maximum_partial_gain_db']*validate(value)*key*band/20)


def weights(midi):
    c=CONTRAST['register_map'];s=smoothstep
    low=1-float(s((midi-c['low_midi'])/(c['neutral_midi']-c['low_midi'])))
    high=float(s((midi-c['neutral_midi'])/(c['high_midi']-c['neutral_midi'])))
    return low,high


def contrast_gain(hz,h,midi,brightness,contrast,fs=48000):
    existing=register_gain(hz,h,midi,brightness)
    low,high=weights(midi)
    if contrast==0 or (low==0 and high==0):return existing
    a=CONTRAST['low_mid_harmonics'];b=CONTRAST['high_harmonics']
    mid=np.exp(-.5*((h-a['center_partial'])/a['width_partials'])**2)
    upper=np.exp(-.5*((h-b['center_partial'])/b['width_partials'])**2)
    if h==1:mid=upper=0. # Keep the original fundamental weighting.
    extra=(a['max_extra_db']*low*mid+b['max_extra_db']*high*upper)*contrast/100
    extra*=1-float(smoothstep((hz-.25*fs)/(.11*fs)))
    existing_db=20*np.log10(existing)
    budget=REGISTER['register']['maximum_partial_gain_db']
    extra*=max(0.,1-existing_db/budget)
    return 10**((existing_db+extra)/20)


def partial_gain(hz,harmonic,config):
    resonance=sum(weight*np.exp(-.5*(np.log2(hz/center)/width)**2)
        for center,width,weight in zip(config['resonance_centers_hz'],
                                      config['resonance_widths_octaves'],
                                      config['resonance_weights']))
    high=float(smoothstep((harmonic-2)/4))
    high*=float(smoothstep((hz-1200)/1600))
    high*=1-float(smoothstep((hz-6000)/2500))
    db=min(config['maximum_combined_db'],config['resonance_db']*resonance+
           config['high_partials_db']*high)
    return 10**(db/20)


def gain(hz,h,c):
    db=20*np.log10(partial_gain(hz,h,c))
    # Harmonic numbers track the played pitch, unlike the fixed resonances.
    extra=c['harmonic_peaks_db'].get(str(h),0.)
    extra*=1-float(smoothstep((hz-5500)/3000))
    return 10**(min(c['maximum_combined_db'],db+extra)/20)


def envelope(t,gate,c):
    end=min(gate,c['end_ms']/1000)
    w=smoothstep(t/min(.003,end*.15))
    w*=np.exp(-np.maximum(t-.012,0)/.024)
    taper=min(.070,end*.7)
    w*=1-smoothstep((t-taper)/max(end-taper,1e-9))
    w[t>=end]=0
    return w


def rates(midi):
    c=FLUTTER;lo=c['primary_low_anchor_midi'];hi=c['primary_high_anchor_midi']
    if midi<lo:
        # Positive asymptote and zero derivative at the lower anchor.
        x=(lo-midi)/c['below_anchor_rolloff_semitones']
        f=c['below_anchor_floor_hz']+(c['primary_low_anchor_hz']-c['below_anchor_floor_hz'])/(1+x*x)
    else:
        k=float(smoothstep((midi-lo)/(hi-lo)))
        f=c['primary_low_anchor_hz']+(c['primary_high_anchor_hz']-c['primary_low_anchor_hz'])*k
    return f,f*c['secondary_rate_ratio']


def depth_scale(midi):
    c=FLUTTER
    k=float(smoothstep((midi-c['depth_low_midi'])/(c['depth_high_midi']-c['depth_low_midi'])))
    return 1-(1-c['high_register_depth_scale'])*k


def rate_multiplier(t,gate=None):
    held=t if gate is None else np.minimum(t,gate)
    return 1+(RATE_RAMP['tail_flutter_rate_max_multiplier']-1)*smoothstep(held/RATE_RAMP['tail_flutter_rate_ramp_s'])


def integrated_time(t,gate=None):
    """Integral of multiplier, seconds; exact and independent of audio blocks."""
    T=RATE_RAMP['tail_flutter_rate_ramp_s'];m=RATE_RAMP['tail_flutter_rate_max_multiplier']
    held=t if gate is None else np.minimum(t,gate)
    u=np.clip(held/T,0,1)
    integral=held+(m-1)*(T*(u**3-.5*u**4)+np.maximum(held-T,0))
    if gate is not None:integral+=rate_multiplier(np.asarray(gate))*np.maximum(t-gate,0)
    return integral


def layers(t,midi,value,gate=None):
    c=FLUTTER
    amount=validate(value)*depth_scale(midi)
    fade=smoothstep(t/(RATE_RAMP['depth_fade_in_ms']/1000))
    f1,f2=rates(midi)
    elapsed=integrated_time(t,gate)
    primary=(1-np.cos(2*np.pi*f1*elapsed))/2
    secondary=(1-np.cos(2*np.pi*f2*elapsed))/2
    return (1-c['primary_max_attenuation']*amount*fade*primary,
            1-c['secondary_max_attenuation']*amount*fade*secondary)


def tail_gain(t,midi,value,gate=None):
    a,b=layers(t,midi,value,gate)
    return a*b


def calibration(midi,velocity):
    fs=48000;gate=.8;t=np.arange(0,gate+.145,64/fs)
    amp,pressure,_=beta_controls(t,gate,velocity,0,p)
    f0=frequency(midi)
    limit=min(96,int(.43*fs/(f0*2**(20/1200))))
    before=[];after=[];attack=[]
    for h in range(1,limit+1):
        c=np.interp(pressure,grid,bank[:,h-1].real)+1j*np.interp(pressure,grid,bank[:,h-1].imag)
        guard=float(smoothstep((.43*fs-h*f0*2**(20/1200))/(.07*fs)))
        original=c*beta_spectral_shape(f0*h,h,pressure,t,gate,midi,p)*amp*guard
        before.append(abs(original)**2/2)
        g=contrast_gain(f0*h,h,midi,60,100,fs)
        after.append(abs(original*g)**2/2)
        attack.append(gain(f0*h,h,ATTACK))
    a=np.array(before);b=np.array(after);k=np.array(attack)[:,None]
    norm0=np.sqrt(a.sum()/max(b.sum(),1e-30));norm1=np.sqrt((a*k*k).sum()/max((b*k*k).sum(),1e-30))
    w=envelope(t,gate,ATTACK)
    # Sum per-harmonic cross terms; same phase in old/enhanced branches.
    u=(1-w)*norm0;v=w*k*norm1
    A=float(np.sum(b*v*v));B=float(np.sum(2*b*u*v));C=float(np.sum(b*u*u)-np.sum(b*norm0*norm0))
    attack_comp=(-B+np.sqrt(max(B*B-4*A*C,0)))/(2*A) if A>1e-30 else 1.
    energy=np.sum(b*((1-w)*norm0+w*k*norm1*attack_comp)**2,axis=0)
    body=(t>=.08)&(t<.72);brass=(t>=.08)&(t<.18)
    # Old noise references before post-decay; preserve original scale.
    tail=tail_gain(t,midi,100,gate)
    return [float(norm0),float(norm1),float(attack_comp),float(np.sqrt(np.mean(energy[body]))),float(np.sqrt(np.mean(amp[body]**2))),float(np.sqrt(np.mean((energy*tail*tail)[brass])))]


def array(x):
    if isinstance(x,(list,tuple,np.ndarray)):return '{'+','.join(array(v) for v in x)+'}'
    return format(float(x),'.17g')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New header path. Existing files are preserved.")
    args = parser.parse_args()
    velocities = [1, 32, 64, 100, 127]
    values = [[calibration(midi, v) for v in velocities] for midi in range(128)]
    complex_pairs = np.stack([bank.real, bank.imag], axis=-1)
    with args.output.open("x", encoding="utf-8", newline="\n") as out:
        out.write('#pragma once\nnamespace LunaData {\n')
        out.write('inline constexpr double bank[33][96][2]=' + array(complex_pairs) + ';\n')
        out.write('inline constexpr int velocities[5]={1,32,64,100,127};\n')
        out.write('inline constexpr double calibration[128][5][6]=' + array(values) + ';\n}\n')


if __name__ == "__main__":
    main()
