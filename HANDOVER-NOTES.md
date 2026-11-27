# WIP notes: split-screen minigame edges (mario-party-3-recomp#122). Delete this file before any PR.

No code change yet. Findings so far, from the MP3 web page at 1280x720 and the reporter's two 1920x1080 pictures:

- Awful Tower (overlay 0x25): the strip is the screen's last two columns, 318-319, at the right edge of the
  rightmost quarter (Peach's in the reporter's game). Those columns hold a picture like columns 0-1 of the
  screen, not the quarter's own background. Quarter cameras: scissors (0,0,92,240) (93,0,160,240)
  (161,0,228,240) (229,0,320,240), each viewport the whole 320 wide centred at x 59/127/195/263. The
  reporter's picture shows it at x 1671-1679 of 1920 (2 columns), and so do mine at x 1114-1119 of 1280.
- Picking Panic (overlay 0x0F): in the reporter's picture the red/yellow divider lies on lines 238-239, with
  line 237 black, after the halves joined into one view. Cameras: in play (0,0,320,119) and (0,121,320,240),
  divider on line 120. The split line moves up (0..109 / 111..240), then (0,0,320,0) and (0,1,320,240) with a
  zoomed viewport. Not yet reproduced in my frames: shots 3 s apart missed the join. Next: a plan that
  watches camera 0's scissor bottom (float at camera + 0x9C, cameras from the pointer at 0x800A0550,
  0x230 each) and shoots every 0.2 s once it drops below 117.
- Both are what a TV's overscan hid at the 4:3 picture's edge (both scenes are drawn at 4:3, black sides).
  Fix idea, undecided: a native the game's patch calls as such a scene starts, naming the screen's edge
  lines/columns to cover, which mp_wipe_picture_sides (src/wipe.cpp) fills black at the frame's end with
  the 4:3 sides (so sprites are covered too, not only 3D under a camera's scissor as
  camera_view_border_bottom does). The MP3 patch would set it for 0x25 (right 2 columns) and 0x0F
  (bottom 2-3 lines, maybe top 2). Finding the divider sprite in the overlay's asm and hiding it there
  would avoid a black band but the overlay is not decompiled.
- Vine With Me (#133) and Ridiculous Relay (#136) were not reached; Relay's scroll seams look like another
  cause.
