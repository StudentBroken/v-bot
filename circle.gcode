; Circle Test
; Center: (650, 400)
; Radius: 50mm

G21 ; Millimeters
G90 ; Absolute positioning

G0 Z0 F1000 ; Pen Up

G0 X700 Y400 F3000 ; Move to start point

G1 Z5 F1000 ; Pen Down

G2 X700 Y400 I-50 J0 F1000 ; Draw Circle

G0 Z0 F1000 ; Pen Up

G0 X650 Y400 F3000 ; Return to center

M2 ; End
