# ESP32-P4 Allsky Display - Logical Flow Chart

This document contains the logical flow chart for the ESP32-P4 Allsky Display project, showing the program execution flow and decision points.

## Main Program Flow

```mermaid
flowchart TD
    A[Program Start] --> B[Check PSRAM]
    B --> C{PSRAM Enabled?}
    C -->|No| D[Error: PSRAM Required]
    C -->|Yes| E[Initialize Display Hardware]
    
    E --> F[Create DSI Panel Object]
    F --> G[Create Display Object]
    G --> H[Initialize Display]
    H --> I{Display Init Success?}
    I -->|No| J[Error: Display Init Failed]
    I -->|Yes| K[Get Display Dimensions]
    
    K --> L[Allocate Memory Buffers]
    L --> M[Allocate Image Buffer]
    M --> N[Allocate Full Image Buffer]
    N --> O[Allocate Scaling Buffer]
    O --> P{Memory Allocation Success?}
    P -->|No| Q[Error: Memory Allocation Failed]
    P -->|Yes| R[Initialize PPA Hardware]
    
    R --> S{PPA Available?}
    S -->|Yes| T[Hardware Acceleration Enabled]
    S -->|No| U[Software Scaling Fallback]
    T --> V[Connect to WiFi]
    U --> V
    
    V --> W[WiFi Connection Process]
    W --> X{WiFi Connected?}
    X -->|No| Y[Display WiFi Error]
    X -->|Yes| Z[Connect to MQTT]
    
    Z --> AA[MQTT Connection Process]
    AA --> BB[Setup Complete]
    BB --> CC[Enter Main Loop]
    
    CC --> DD[Main Loop]
```

## Main Loop Flow

```mermaid
flowchart TD
    A[Main Loop Start] --> B[Check WiFi Status]
    B --> C{WiFi Connected?}
    C -->|No| D[Attempt WiFi Reconnection]
    C -->|Yes| E[Handle MQTT]
    
    D --> F{Reconnection Success?}
    F -->|No| A
    F -->|Yes| E
    
    E --> G[MQTT Connection Check]
    G --> H{MQTT Connected?}
    H -->|No| I[Attempt MQTT Reconnection]
    H -->|Yes| J[Process MQTT Messages]
    
    I --> K[Process Serial Commands]
    J --> K
    
    K --> L[Check Serial Input]
    L --> M{Serial Command Available?}
    M -->|Yes| N[Parse Command]
    M -->|No| O[Check Update Timer]
    
    N --> P[Execute Image Transform]
    P --> O
    
    O --> Q{Time for Image Update?}
    Q -->|No| R[Delay 100ms]
    Q -->|Yes| S{First Image?}
    
    S -->|Yes| T[Download with Debug]
    S -->|No| U[Download Silent]
    
    T --> V[Image Processing]
    U --> V
    V --> R
    R --> A
```

## Image Download and Processing Flow

```mermaid
flowchart TD
    A[Start Image Download] --> B[Create HTTP Client]
    B --> C[Send GET Request]
    C --> D{HTTP Response OK?}
    D -->|No| E[Log Error]
    D -->|Yes| F[Get Content Size]
    
    F --> G{Size Valid?}
    G -->|No| H[Log Size Error]
    G -->|Yes| I[Download Image Data]
    
    I --> J[Read Data Stream]
    J --> K{All Data Received?}
    K -->|No| J
    K -->|Yes| L[JPEG Decode Process]
    
    L --> M[Initialize JPEG Decoder]
    M --> N[Open JPEG from RAM]
    N --> O{JPEG Valid?}
    O -->|No| P[Log JPEG Error]
    O -->|Yes| Q[Get Image Dimensions]
    
    Q --> R{Fits in Buffer?}
    R -->|No| S[Log Buffer Error]
    R -->|Yes| T[Decode to Full Buffer]
    
    T --> U[Image Rendering Process]
    U --> V[Calculate Transformations]
    V --> W[Apply Scale and Offset]
    W --> X[Render to Display]
    X --> Y[Mark First Image Loaded]
    Y --> Z[End]
    
    E --> Z
    H --> Z
    P --> Z
    S --> Z
```

## Image Rendering and Scaling Flow

```mermaid
flowchart TD
    A[Start Image Rendering] --> B[Calculate Scaled Dimensions]
    B --> C[Calculate Display Position]
    C --> D{Scaling Required?}
    D -->|No| E[Direct Copy to Display]
    D -->|Yes| F{PPA Available?}
    
    F -->|Yes| G[Hardware Scaling Process]
    F -->|No| H[Software Scaling Process]
    
    G --> I[Copy to PPA Source Buffer]
    I --> J[Configure PPA Operation]
    J --> K[Execute PPA Scaling]
    K --> L{PPA Success?}
    L -->|No| H
    L -->|Yes| M[Copy from PPA Destination]
    
    H --> N{Image Fits in Buffer?}
    N -->|Yes| O[Scale Entire Image]
    N -->|No| P[Strip-based Scaling]
    
    O --> Q[Bilinear Interpolation]
    P --> R[Process Image Strips]
    R --> S[Scale Each Strip]
    S --> T[Bilinear Interpolation]
    T --> U[Draw Strip to Display]
    U --> V{More Strips?}
    V -->|Yes| R
    V -->|No| W[Scaling Complete]
    
    Q --> X[Draw to Display]
    M --> X
    E --> X
    X --> W
    W --> Y[End]
```

## MQTT Message Handling Flow

```mermaid
flowchart TD
    A[MQTT Message Received] --> B[Parse Topic]
    B --> C{Brightness Topic?}
    C -->|No| D[Ignore Message]
    C -->|Yes| E[Parse Message Payload]
    
    E --> F[Convert to Integer]
    F --> G{Valid Range 0-255?}
    G -->|No| H[Log Invalid Value]
    G -->|Yes| I[Set Display Brightness]
    
    I --> J[Update Brightness Variable]
    J --> K[Apply Hardware Brightness]
    K --> L[Send LCD Command]
    L --> M[Log Success]
    M --> N[End]
    
    D --> N
    H --> N
```

## Serial Command Processing Flow

```mermaid
flowchart TD
    A[Serial Command Received] --> B[Read Command Character]
    B --> C{Command Type?}
    
    C -->|+/-| D[Scale Both Axes]
    C -->|X/Z| E[Scale X Axis]
    C -->|Y/U| F[Scale Y Axis]
    C -->|W/S| G[Move Vertical]
    C -->|A/D| H[Move Horizontal]
    C -->|R| I[Reset Transform]
    C -->|H/?| J[Show Help]
    C -->|Other| K[Ignore Command]
    
    D --> L[Update Scale Factors]
    E --> M[Update X Scale]
    F --> N[Update Y Scale]
    G --> O[Update Y Offset]
    H --> P[Update X Offset]
    I --> Q[Reset All Values]
    J --> R[Print Help Text]
    
    L --> S[Redraw Image]
    M --> S
    N --> S
    O --> S
    P --> S
    Q --> S
    R --> T[End]
    K --> T
    S --> T
```

## Touch Interface Flow (GT911)

```mermaid
flowchart TD
    A[Touch System Init] --> B[Initialize I2C Bus]
    B --> C[Configure GT911 Settings]
    C --> D[Reset Touch Controller]
    D --> E[Read Configuration]
    E --> F{Init Success?}
    F -->|No| G[Touch Disabled]
    F -->|Yes| H[Touch Ready]
    
    H --> I[Touch Event Loop]
    I --> J[Read Touch Data]
    J --> K{Touch Detected?}
    K -->|No| L[Clear Touch State]
    K -->|Yes| M[Get Coordinates]
    
    M --> N[Process Touch Points]
    N --> O[Apply Coordinate Transform]
    O --> P[Update Touch Data]
    P --> Q[Trigger Callback]
    Q --> R{Continue Monitoring?}
    R -->|Yes| I
    R -->|No| S[End]
    
    L --> R
    G --> S
```

## Error Handling and Recovery Flow

```mermaid
flowchart TD
    A[Error Detected] --> B{Error Type?}
    
    B -->|PSRAM| C[Fatal: Restart Required]
    B -->|Display Init| D[Fatal: Hardware Issue]
    B -->|Memory Alloc| E[Fatal: Insufficient Memory]
    B -->|WiFi| F[Recoverable: Retry Connection]
    B -->|MQTT| G[Recoverable: Retry Connection]
    B -->|HTTP| H[Recoverable: Skip Update]
    B -->|JPEG| I[Recoverable: Skip Image]
    B -->|PPA| J[Recoverable: Use Software]
    
    C --> K[Log Error and Halt]
    D --> K
    E --> K
    
    F --> L[WiFi Reconnection]
    G --> M[MQTT Reconnection]
    H --> N[Continue Operation]
    I --> N
    J --> O[Switch to Software Mode]
    
    L --> P{Success?}
    M --> Q{Success?}
    P -->|Yes| N
    P -->|No| R[Retry Later]
    Q -->|Yes| N
    Q -->|No| R
    
    O --> N
    R --> N
    N --> S[Resume Normal Operation]
    S --> T[End]
    K --> T
```

## Memory Management Flow

```mermaid
flowchart TD
    A[Memory Management] --> B[Check Available PSRAM]
    B --> C{Sufficient PSRAM?}
    C -->|No| D[Error: Insufficient Memory]
    C -->|Yes| E[Allocate Image Buffer]
    
    E --> F[Allocate Full Image Buffer]
    F --> G[Allocate Scaling Buffer]
    G --> H{PPA Available?}
    H -->|Yes| I[Allocate PPA Buffers]
    H -->|No| J[Skip PPA Allocation]
    
    I --> K[Align PPA Buffers]
    K --> L[Verify Buffer Alignment]
    L --> M{Alignment OK?}
    M -->|No| N[Free PPA Buffers]
    M -->|Yes| O[Memory Setup Complete]
    
    J --> O
    N --> P[Disable PPA]
    P --> O
    O --> Q[Monitor Memory Usage]
    Q --> R[End]
    D --> R
```

## Key Decision Points and States

### Critical System States
1. **Initialization Phase**: Hardware setup and memory allocation
2. **Connection Phase**: WiFi and MQTT establishment
3. **Operational Phase**: Image downloading and display
4. **Error Recovery**: Handling failures and reconnections

### Performance Optimizations
1. **Hardware Acceleration**: PPA usage for image scaling
2. **Memory Management**: PSRAM utilization for large buffers
3. **Streaming**: Progressive image download and processing
4. **Caching**: Full image buffer for smooth transformations

### Error Recovery Mechanisms
1. **Connection Recovery**: Automatic WiFi/MQTT reconnection
2. **Graceful Degradation**: Software fallback when hardware fails
3. **Memory Protection**: Buffer size validation and overflow prevention
4. **Timeout Handling**: Network operation timeouts and retries
