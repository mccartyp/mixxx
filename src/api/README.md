# Mixxx REST API

A minimal REST API service for Mixxx that provides status information and playback control.

## Configuration

The REST API server can be configured in the Mixxx configuration file:

```ini
[RestApi]
Port=8080
```

Default port: **8080**
Default host: **localhost** (127.0.0.1)

## API Endpoints

### Get Overall Status
**GET** `/api/status`

Returns status information for all players and master controls.

**Response:**
```json
{
  "players": [
    {
      "group": "[Channel1]",
      "track": {
        "artist": "Artist Name",
        "title": "Track Title",
        "album": "Album Name",
        "duration": 180.5,
        "bpm": 128.0,
        "key": "Am",
        ...
      },
      "play": 1.0,
      "playposition": 0.5,
      "duration": 180.5,
      "volume": 1.0,
      "bpm": 128.0,
      ...
    }
  ],
  "master": {
    "volume": 1.0,
    "balance": 0.0,
    "headVolume": 1.0,
    "headMix": 0.0
  }
}
```

### Get Player Status
**GET** `/api/player/:group`

Returns detailed status for a specific player.

**Parameters:**
- `group` - Player group name (e.g., `Channel1`, `Channel2`, `Sampler1`)
  - Brackets are optional: both `/api/player/Channel1` and `/api/player/[Channel1]` work

**Response:**
```json
{
  "track": {
    "artist": "Artist Name",
    "title": "Track Title",
    "album": "Album Name",
    "album_artist": "Album Artist",
    "genre": "Electronic",
    "composer": "Composer Name",
    "year": "2023",
    "comment": "Comment",
    "duration": 180.5,
    "bpm": 128.0,
    "key": "Am",
    "location": "/path/to/track.mp3",
    "file_type": "mp3"
  },
  "play": 1.0,
  "play_indicator": 1.0,
  "playposition": 0.5,
  "duration": 180.5,
  "volume": 1.0,
  "pregain": 1.0,
  "bpm": 128.0,
  "rate": 0.0,
  "tempo_ratio": 1.0,
  "keylock": 0.0,
  "repeat": 0.0,
  "loop_enabled": 0.0,
  "track_loaded": 1.0
}
```

### Get Control Value
**GET** `/api/control/:group/:item`

Returns the current value of a specific control.

**Parameters:**
- `group` - Control group (e.g., `[Channel1]`, `[Master]`)
- `item` - Control item name (e.g., `play`, `volume`, `rate`)

**Response:**
```json
{
  "group": "[Channel1]",
  "item": "play",
  "value": 1.0
}
```

### Set Control Value
**POST** `/api/control/:group/:item`

Sets the value of a specific control.

**Parameters:**
- `group` - Control group (e.g., `[Channel1]`, `[Master]`)
- `item` - Control item name (e.g., `play`, `volume`, `rate`)

**Request Body:**
```json
{
  "value": 1.0
}
```

**Response:**
```json
{
  "success": true,
  "group": "[Channel1]",
  "item": "play",
  "value": 1.0
}
```

## Common Controls

### Playback Controls (per deck: `[Channel1]`, `[Channel2]`, etc.)

| Control | Description | Values |
|---------|-------------|--------|
| `play` | Play/pause toggle | 0 (stopped), 1 (playing) |
| `stop` | Stop playback | 1 (trigger) |
| `playposition` | Current playback position | 0.0 - 1.0 (normalized) |
| `duration` | Track duration | Seconds |
| `volume` | Volume level | 0.0 - 1.0 |
| `pregain` | Pre-gain/input level | 0.0 - 4.0 |
| `rate` | Playback rate | -1.0 to +1.0 |
| `bpm` | Current BPM | BPM value |
| `tempo_ratio` | Tempo ratio | Multiplier |
| `keylock` | Key lock toggle | 0 (off), 1 (on) |
| `repeat` | Repeat mode | 0 (off), 1 (on) |
| `loop_enabled` | Loop enabled | 0 (off), 1 (on) |

### Master Controls (`[Master]`)

| Control | Description | Values |
|---------|-------------|--------|
| `volume` | Master volume | 0.0 - 1.0 |
| `balance` | Master balance | -1.0 to +1.0 |
| `headVolume` | Headphone volume | 0.0 - 1.0 |
| `headMix` | Headphone mix | 0.0 - 1.0 |

## Example Usage

### Using curl

Get status of all players:
```bash
curl http://localhost:8080/api/status
```

Get status of Channel 1:
```bash
curl http://localhost:8080/api/player/Channel1
```

Play Channel 1:
```bash
curl -X POST http://localhost:8080/api/control/Channel1/play \
  -H "Content-Type: application/json" \
  -d '{"value": 1.0}'
```

Pause Channel 1:
```bash
curl -X POST http://localhost:8080/api/control/Channel1/play \
  -H "Content-Type: application/json" \
  -d '{"value": 0.0}'
```

Set volume to 50%:
```bash
curl -X POST http://localhost:8080/api/control/Channel1/volume \
  -H "Content-Type: application/json" \
  -d '{"value": 0.5}'
```

Get current playback position:
```bash
curl http://localhost:8080/api/control/Channel1/playposition
```

### Using JavaScript/fetch

```javascript
// Get status
const response = await fetch('http://localhost:8080/api/status');
const status = await response.json();
console.log(status);

// Play a track
await fetch('http://localhost:8080/api/control/Channel1/play', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ value: 1.0 })
});

// Get track metadata
const playerResponse = await fetch('http://localhost:8080/api/player/Channel1');
const player = await playerResponse.json();
console.log(`Now playing: ${player.track.artist} - ${player.track.title}`);
```

### Using Python

```python
import requests

# Get status
response = requests.get('http://localhost:8080/api/status')
status = response.json()
print(status)

# Play a track
requests.post('http://localhost:8080/api/control/Channel1/play',
    json={'value': 1.0})

# Get track info
response = requests.get('http://localhost:8080/api/player/Channel1')
player = response.json()
if player['track']:
    print(f"Now playing: {player['track']['artist']} - {player['track']['title']}")
```

## CORS Support

The API includes CORS headers to allow cross-origin requests from web applications:
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type`

## Architecture

The REST API server is implemented using Qt's `QTcpServer` for HTTP handling and integrates with Mixxx's control object system:

- **RestApiServer** (`src/api/restapiserver.h/.cpp`) - Main HTTP server implementation
- **ControlObject** - Read/write control values
- **PlayerInfo** - Access current track information
- **Track** - Access track metadata

The server is initialized during CoreServices startup and runs on a separate thread to avoid blocking the audio engine.

## Security Notes

- The API server binds to **localhost only** by default for security
- No authentication is currently implemented
- This is intended for local automation and integration, not remote access
- To expose remotely, use a reverse proxy with proper authentication

## Performance

The REST API is designed to be lightweight:
- HTTP parsing is minimal
- Control access uses Mixxx's thread-safe ControlObject system
- No blocking operations on the audio thread
- JSON serialization is performed on-demand

## Future Enhancements

Potential future additions:
- WebSocket support for real-time updates
- Bulk control operations
- Library search and browsing
- Playlist management
- Effect control
- Authentication and authorization
- HTTPS support
