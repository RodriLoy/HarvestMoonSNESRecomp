# Shader presets of this port

Picked in the launcher (Settings → Shader). Written for this project,
public domain / CC0-1.0.

| Preset | What it does |
|---|---|
| `pixel-sharp-4k` | Crisp square pixels at any window or screen size, without the uneven pixels of plain nearest-neighbour scaling. |
| `pixel-smooth-4x` | Two EPX/Scale2x steps (4× the detail on diagonals and curves), then Pixel Sharp to the screen. |

The framework's own presets (CRT Soft, LCD Grid, Sharp, Warm Composite) are
staged beside these.
