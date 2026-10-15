# Hutara

VCV Rack 2 plugin with four darkpsy / psytrance modules.

## Modules

| Slug | Name | Description |
|---|---|---|
| `FmOperator` | Hutara PsyOperator | VCO with FM synthesis, waveshaping and wavefolding |
| `PsyOperatorFX` | Hutara PsyOperator FX | Modulation-driven performance processor: varispeed tape, granular, slicer, trance gate, scatter |
| `Hutara_Random_CV` | Hutara Random | Sample & Hold, random CV and gate with envelope generator |
| `PsyKick` | Hutara VOIDKICK | Psytrance kick and rolling bass voice |

## Build

Requires the [Rack SDK](https://vcvrack.com/manual/Building#Building-Rack-plugins).

```bash
export RACK_DIR=/path/to/Rack-SDK
make
make dist
make install
```

## License

GPL-3.0-only. See [LICENSE](LICENSE).
