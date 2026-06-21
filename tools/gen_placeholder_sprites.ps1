<#
    gen_placeholder_sprites.ps1

    Generates the simple 2D pixel-art placeholder sprites the game loads from
    assets/. Run it from anywhere; it writes PNGs into ..\assets relative to
    this script.

        pwsh tools/gen_placeholder_sprites.ps1

    These are intentionally crude stand-ins (Enter-the-Gungeon-ish chunky pixel
    art). Artists can either:
      * replace the PNGs in assets/ directly (no code changes, no rebuild), or
      * tweak the draw routines below and re-run this script.

    Each sprite is authored at a small native resolution and scaled up in-game
    with nearest-neighbour filtering, which keeps the pixels crisp/chunky. The
    AUTHORED size is roughly half (or a third) of the in-game draw size so the
    scale factor stays close to an integer. See assets/README.md for the
    AssetKind -> filename mapping and the in-game draw sizes.
#>

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'Stop'

$assetsDir = Join-Path $PSScriptRoot '..\assets'
$assetsDir = [System.IO.Path]::GetFullPath($assetsDir)

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    # No anti-aliasing: we want hard pixel edges, not smooth blends.
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    return @{ Bmp = $bmp; G = $g }
}

function C([int]$r, [int]$g, [int]$b, [int]$a = 255) {
    return [System.Drawing.Color]::FromArgb($a, $r, $g, $b)
}

function Fill($g, $color, [int]$x, [int]$y, [int]$w, [int]$h) {
    $brush = New-Object System.Drawing.SolidBrush $color
    $g.FillRectangle($brush, $x, $y, $w, $h)
    $brush.Dispose()
}

function Ellipse($g, $color, [int]$x, [int]$y, [int]$w, [int]$h) {
    $brush = New-Object System.Drawing.SolidBrush $color
    $g.FillEllipse($brush, $x, $y, $w, $h)
    $brush.Dispose()
}

# Filled rectangle with a 1px outline — the building block for the chunky,
# outlined pixel-art look.
function ORect($g, $fill, $outline, [int]$x, [int]$y, [int]$w, [int]$h) {
    Fill $g $fill $x $y $w $h
    $pen = New-Object System.Drawing.Pen $outline, 1
    $g.DrawRectangle($pen, $x, $y, ($w - 1), ($h - 1))
    $pen.Dispose()
}

function Save($canvas, [string]$name) {
    $path = Join-Path $assetsDir $name
    $canvas.G.Dispose()
    $canvas.Bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Bmp.Dispose()
    Write-Host "wrote $path"
}

# ---- Palette ---------------------------------------------------------------
$K  = C 24 22 30        # near-black outline
$W  = C 236 240 245     # white highlight
$rb = C 96 150 210      # player robe (light)
$rB = C 60 98 150       # player robe (dark)
$sk = C 240 200 160     # skin
$gd = C 242 210 90      # gold trim
$er = C 212 64 64       # enemy red (light)
$eR = C 150 32 32       # enemy red (dark)
$ey = C 250 232 84      # enemy eye
$am = C 184 72 98       # vendor awning red
$ap = C 246 222 124     # vendor awning stripe / gold
$wd = C 96 64 38        # vendor wood (dark)
$wl = C 132 92 56       # vendor wood (light)
$vb = C 86 130 210      # vendor keeper body
$ff = C 46 46 58        # floor base
$fg = C 58 58 72        # floor grout (light)
$fd = C 34 34 44        # floor speck (dark)
$sa = C 255 236 184     # slash bright
$so = C 255 130 50      # slash orange
# Tech-fantasy player palette.
$pK = C 18 16 28        # player outline (near-black navy)
$ci = C 64 60 120       # coat indigo
$cd = C 44 40 86        # coat indigo (shadow)
$cy = C 96 222 236      # tech cyan glow
$cw = C 198 250 255     # bright cyan highlight
$mg = C 190 120 235     # magenta energy accent
$hl = C 100 110 142     # helmet steel
$hd = C 62 70 96        # helmet steel (shadow)
$bt = C 44 48 66        # boots / gloves
$pt = C 74 78 110       # trouser (front leg)
$pd = C 52 56 84        # trouser (back leg, shadow)

# ---- player.png (32x32, side view, faces +x / right) -----------------------
# A side-on "tech-fantasy" adventurer: a hooded/helmeted mage in an indigo coat
# with glowing cyan tech trim and a visor. The game flips this left/right (never
# rotates it), so it is authored as a standing side view facing RIGHT.
$c = New-Canvas 32 32
# Legs + boots (back leg first, slightly darker, so the front leg reads ahead).
ORect $c.G $pd $pK 12 22 4 6        # back leg
ORect $c.G $bt $pK 11 27 5 4        # back boot
ORect $c.G $pt $pK 16 22 4 6        # front leg
ORect $c.G $bt $pK 16 27 6 4        # front boot
# Back arm tucked behind the coat.
ORect $c.G $cd $pK 10 13 4 8        # back arm sleeve
ORect $c.G $bt $pK 10 20 4 3        # back glove
# Coat / torso with a flared hem.
ORect $c.G $ci $pK 11 12 11 12      # torso
ORect $c.G $ci $pK 10 21 14 4       # coat hem flare
Fill  $c.G $cd 11 12 4 12           # shaded back half of the coat
Fill  $c.G $mg 10 13 1 11           # magenta energy seam down the back
# Glowing cyan tech trim + chest core.
Fill  $c.G $cy 19 13 1 9            # front trim line
Fill  $c.G $cw 17 15 3 3            # chest core (bright)
Fill  $c.G $cy 16 16 1 1            # core glow
# Belt with a glowing buckle.
ORect $c.G $bt $pK 11 19 12 2       # belt
Fill  $c.G $cy 16 19 3 2            # buckle
# Front arm reaching forward, with a glowing gauntlet.
ORect $c.G $ci $pK 18 13 5 8        # front sleeve
ORect $c.G $bt $pK 19 19 4 4        # front glove
Fill  $c.G $cy 19 19 4 1            # gauntlet glow
# Head: steel helmet with a glowing visor and a small antenna fin.
ORect $c.G $hl $pK 11 3 11 10       # helmet
Fill  $c.G $hd 11 3 4 10            # helmet back (shadow)
Fill  $c.G $cw 13 4 4 1             # helmet top highlight
Fill  $c.G $sk 18 9 3 3             # jaw / chin showing at the front
Fill  $c.G $cy 16 6 5 3             # visor
Fill  $c.G $cw 19 6 1 1             # visor glint
Fill  $c.G $cy 13 0 2 3            # antenna fin (top-back)
Save $c 'player.png'

# ---- enemy.png (22x22) -----------------------------------------------------
# A blob creature with two eyes and a jagged mouth. Authored LIGHT/neutral on
# purpose: the game tints this sprite per enemy type (see EnemyTypes.cpp), and a
# light base means a colour multiply produces that type's colour cleanly. Dark
# eyes/mouth stay dark under any tint.
$eBody  = C 226 228 234   # light body
$eShade = C 188 190 202   # lower shading
$eDark  = C 40 42 54      # eyes / mouth
$c = New-Canvas 22 22
Ellipse $c.G $K     1 2 20 19   # body outline
Ellipse $c.G $eBody 2 3 18 17   # body
Ellipse $c.G $eShade 2 11 18 9  # lower shading
Fill    $c.G $eDark 6 8 4 4     # left eye
Fill    $c.G $eDark 12 8 4 4    # right eye
Fill    $c.G $eDark 6 15 3 2    # mouth
Fill    $c.G $eDark 10 16 3 2   # mouth
Fill    $c.G $eDark 13 15 3 2   # mouth
Save $c 'enemy.png'

# ---- vendor.png (32x40) ----------------------------------------------------
# A little market stall: striped awning, wooden counter, keeper behind it.
$c = New-Canvas 32 40
# Keeper (drawn first so the counter overlaps the lower body).
Ellipse $c.G $K  10 10 12 12    # head outline
Ellipse $c.G $sk 11 11 10 10    # head
Fill    $c.G $vb 11 19 10 12    # body
# Counter.
Fill    $c.G $wd 3 28 26 9      # counter front
Fill    $c.G $wl 3 28 26 2      # counter top edge
# Awning.
Fill    $c.G $am 2 2 28 9       # awning base
for ($x = 3; $x -lt 29; $x += 8) { Fill $c.G $ap $x 2 4 9 }   # stripes
Fill    $c.G $K  2 2 28 9 | Out-Null
$pen = New-Object System.Drawing.Pen $K, 1
$c.G.DrawRectangle($pen, 2, 2, 27, 8)      # awning outline
$c.G.DrawRectangle($pen, 3, 28, 25, 8)     # counter outline
$pen.Dispose()
# Posts holding the awning up.
Fill    $c.G $wl 3 11 2 17
Fill    $c.G $wl 27 11 2 17
Save $c 'vendor.png'

# ---- floor.png (32x32, seamless tile) --------------------------------------
# Dark stone tile. A light grout line on the top/left edges reads as a grid
# when the tile is repeated across the room floor; a few specks add texture.
$c = New-Canvas 32 32
Fill $c.G $ff 0 0 32 32         # base
Fill $c.G $fg 0 0 32 1          # top grout
Fill $c.G $fg 0 0 1 32          # left grout
Fill $c.G $fd 8 20 2 2          # specks (kept off edges to stay seamless)
Fill $c.G $fd 22 10 2 2
Fill $c.G $fd 15 15 2 2
Fill $c.G $fd 25 24 2 2
Save $c 'floor.png'

# ---- attack.png (multi-frame sprite sheet, slash faces +x / right) ----------
# A crescent melee slash, animated as a sword-swing sweeping from high to low.
#
# This is a horizontal SPRITE SHEET: $frames square cells laid out left-to-right,
# each cell $fs x $fs. The game reads the sheet height as the frame size and
# (width / height) as the frame count, then plays one cell per slice of the
# swing. To change the animation, just edit the loop below (or hand-draw frames)
# and keep the cells square and in a single row -- no code change needed.
$frames = 6
$fs     = 40
$c = New-Canvas ($frames * $fs) $fs
for ($i = 0; $i -lt $frames; $i++) {
    $fx = $i * $fs
    $t  = if ($frames -gt 1) { $i / ($frames - 1) } else { 0 }
    # Sweep the crescent's centre angle from up (-70 deg) to down (+70 deg).
    $center = -70 + 140 * $t
    # Fade in then out so the swing has a soft start and trailing finish.
    $fade = 0.4 + 0.6 * [Math]::Sin([Math]::PI * (($i + 0.5) / $frames))
    $aO = [int](235 * $fade)
    $aI = [int](255 * $fade)
    $colO = C 255 130 50 $aO
    $colI = C 255 236 184 $aI
    $penO = New-Object System.Drawing.Pen $colO, 7
    $penO.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $penO.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $penI = New-Object System.Drawing.Pen $colI, 3
    $penI.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $penI.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    # Bounding box centred in this frame cell; arc opens toward +x (right).
    $c.G.DrawArc($penO, ($fx + 6), 4, 28, 32, ($center - 48), 96)
    $c.G.DrawArc($penI, ($fx + 6), 4, 28, 32, ($center - 40), 80)
    $penO.Dispose(); $penI.Dispose()
}
Save $c 'attack.png'

Write-Host "All placeholder sprites written to $assetsDir"
