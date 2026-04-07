; Square 100x100mm (Relative)
G21 ; Millimeters
G91 ; Relative positioning
M3  ; Pen Down
G1 X100 F2000 ; East 100mm
G1 Y100       ; South 100mm
G1 X-100      ; West 100mm
G1 Y-100      ; North 100mm
M5  ; Pen Up
G90 ; Revert to Absolute positioning
