; homex.g
; called to home the X axis
;
; generated by RepRapFirmware Configuration Tool on Tue Oct 23 2018 00:51:50 GMT+0200 (hora de verano de Europa central)
G91               ; relative positioning
G1 Z5 F6000 S2    ; lift Z relative to current position
G1 S1 X-325 F3000 ; move quickly to X axis endstop and stop there (first pass)
G1 X5 F6000       ; go back a few mm
G1 S1 X-325 F360  ; move slowly to X axis endstop once more (second pass)
G1 Z-5 F6000 S2   ; lower Z again
G1 X10 F6000
G90               ; absolute positioning