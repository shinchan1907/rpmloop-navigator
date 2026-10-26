# rpmloop — Hunter 350 dashboard, P3

**Order status: FIT-TEST PROTOTYPE ONLY. Units: millimetres. Print at 100% scale.**

USB-C is on the **bottom (-Y), left of the action-camera mount**, with the ESP32 and its cradle rotated to match. The mount has **two fingers**, one centre gap and an M5 through-hole. The mating accessory supplies the nut receiver; there is no third finger or external nut collar on this enclosure.

The front Instagram-style icon and `rpmloop` wordmark are retained. A second, large **RPMLOOP** wordmark is recessed into the **top outer wall (+Y)**: 10 mm capital height and 0.6 mm engraving depth. The main 3 mm wall retains approximately 2.4 mm beneath this engraving. It does not cover the screen.

The supplied files are complete for the **modelled prototype parts**. They are not a qualified rainproof or road-load production release. Before ordering an expensive final set, check the actual LCD dimensions, insert outside diameter, wired-board fit, USB plug and mating mount. Those measurements were not supplied, and their CAD parameters remain explicitly unverified.

## Folder contents

- `01_Rigid_STL/`: individually separated ASA parts and an insert-fit coupon.
- `02_Flexible_Prototype_STL/`: uncompressed gasket and washer manufacture masters. **Never print these in ASA.**
- `03_Images/`: assembled, bottom-port, internal and individual-part views.
- `04_CAD/`: native editable SolidCraft assembly, assembly STEP and individual part STEP files. STEP coordinates preserve assembly positions.
- `05_Verification/`: digital checks, interference results and source inspection evidence.
- `06_Source/`: reproducible Python generators and the frozen SolidCraft command script.
- `parts_and_hardware.csv`: quantities and purchasing/printing notes.
- `SHA256SUMS.txt`: hashes for checking that files belong to this package.

STLs are translated to a flat print datum, with minimum Z = 0 and XY centred. No scaling is applied. They are **individual upload parts**, not an assembly plate. File names include the required quantity; check that quantity in the printing cart.

## Printing order — rigid parts

These are starting settings for a 0.4 mm nozzle. The supplier must use an enclosed ASA process and calibrate shrinkage; do not scale the files arbitrarily. All rigid parts are **matte black**. The cradle appears grey in some review images only to make it easier to inspect.

| File stem | Qty | Material / colour | Layer | Walls | Infill | Nominal thickness / key feature | Orientation and support |
|---|---:|---|---:|---:|---|---|---|
| `housing_qty1` | 1 | ASA, matte black | 0.20 mm | 6 | 35% gyroid; local solid modifier in mount root/fingers | Main walls 3 mm; sealing lands 4.5 mm; mount fingers 3 mm; insert pilots currently 2.8 mm | Rear joint face down, front up. Local removable supports at mount as needed. Keep support scars off sealing lands. |
| `front_bezel_qty1` | 1 | ASA, matte black | 0.16–0.20 mm | 5 | 35% gyroid | 4 mm plate; 1.6 mm window rebate; recessed branding 0.5 mm | Flat rear face down; opening/rebate up. No supports normally needed. |
| `rear_lid_qty1` | 1 | ASA, matte black | 0.20 mm | 5 | 35% gyroid | 3 mm plate; 2 mm head recesses leave 1 mm floor | Exterior face down, flat sealing face up. Check small recess bridges; avoid supports on seal face. |
| `esp32_cradle_qty1` | 1 | ASA, matte black | 0.16 mm | 4 | 100% | Base 1.6 mm; board outline 18 × 23.5 mm; provisional PCB slide gap 1.4 mm | Underside down. Small guide lips may need local support; remove it without damaging the groove. |
| `PCB_tab_left_qty2` | 2 | ASA, matte black | 0.16–0.20 mm | 4 | 100% | 2 mm thick; 7 × 5 mm outline; M2 clearance | Flat face down. No supports. |
| `PCB_tab_right_qty2` | 2 | ASA, matte black | 0.16–0.20 mm | 4 | 100% | 2 mm thick; opposite-handed screw position | Flat face down. No supports. |
| `insert_test_coupon_qty1` | 1 | **Same ASA and process as housing**, matte black | 0.20 mm | 5 | 100% | 38 × 14 × 7 mm; blind trial holes 2.6 / 2.8 / 3.0 / 3.2 mm | Flat underside down, holes up. Test actual M2 × 4 mm inserts here first. |

For the coupon, hole sizes run from **2.6 to 3.2 mm from negative X to positive X** in CAD. The two different tab files are handed parts: print two of each, not four of only one.

The housing mount is integral, not an additional print. Use a metal M5 thumb screw and the mating accessory's metal nut. Place the **thumb knob on +X, opposite the USB cable**. The hidden review gauge allows a 24 mm knob at X = 16…22 mm; the actual fastener must match this clearance or be rechecked. The nominal three-finger mating gauge is inspection geometry only and is not included among print uploads.

## Gaskets and washers — flexible parts, not ASA

**Preferred final sealing material: black silicone, approximately Shore A 40–50, selected and fabricated by a seal supplier.** Ordinary FDM printers do not print this silicone directly. The supplied STL geometry can be used for quoting/casting or a flexible fit prototype. For an FDM fit prototype, use **TPU 85A, black, 0.10–0.15 mm layers, 100% infill, flat on the bed, no supports**. A TPU fit prototype does not establish rain sealing, compression set or long-term ASA compatibility.

| File stem | Qty | Uncompressed thickness | Cross-section / outline | Use and restrictions |
|---|---:|---:|---|---|
| `joint_gasket_qty2` | 2 | **1.5 mm** | Flat continuous ring, 1.3 mm wide; outer 96.8 × 66.8 mm, inner 94.2 × 64.2 mm, rounded corners | One front and one rear. Matching housing grooves are 2.1 mm wide × 1.15 mm deep. Nominal height compression 23.3%; cross-section fill approximately 80.75%. |
| `display_gasket_qty1` | 1 | **0.8 mm** | 1.3 mm ring width; outer 65.4 × 47.4 mm, inner 62.8 × 44.8 mm | **UNVERIFIED display interface.** Only a template until active area, glass and touch borders are measured. Must seal to a stationary surface without loading active pixels or resistive touch. |
| `USB_cap_gasket_qty1` | 1 optional | **1.0 mm** | 1 mm ring width; outer 18.5 × 12.5 mm, inner 16.5 × 10.5 mm | Template for the port cap interface. **Do not order as a final seal:** a fitted cap and powered-cable boot are not designed without the actual connector/plug dimensions. |
| `M2_sealing_washer_qty8` | 8 | **0.5 mm** | OD 4.8 mm, ID 2.4 mm | Prefer purchased silicone washers matching the screw heads. CAD shows approximately 0.4 mm compressed height. Verify head overlap and compression. |

**Do not print the compressed gasket bodies visible in the assembly.** The flexible STLs use separate, uncompressed masters. Do not print reference LCDs, PCBs, battery/GPS volumes, cable envelopes or mating gauges.

The joint groove was widened during P3 review. The volume check is informed by [Parker's O-Ring Handbook](https://www.parker.com/content/dam/Parker-com/Literature/O-Ring-Division-Literature/ORD-5700.pdf), which describes gland-fill allowances for elastomer seals. Applying a suitable volume allowance is a design check, not qualification of this particular flat silicone/TPU gasket. Material, finish, compression force and leak testing still matter.

## Hardware to purchase and verify

| Item | Quantity | Specification / check |
|---|---:|---|
| Brass heat-set inserts | 14 | M2 internal thread × 4 mm length. **Outside diameter and supplier pilot recommendation unknown. Current 2.8 mm CAD pilot is provisional.** 8 enclosure positions, 4 display-tab positions, 2 cradle positions. |
| Front bezel screws | 4 | M2 × 6 mm candidate, suitable low-profile/pan head; verify actual head height, washer and engagement. |
| Rear cover screws | 4 | M2 × 5 mm candidate; do not bottom in blind pockets. |
| Display-tab screws | 4 | M2 × 5 mm candidate for 2 mm tabs; verify engagement with actual inserts. |
| ESP32 cradle screws | 2 | M2 × 5 mm candidate. |
| Silicone screw sealing washers | 8 | See washer specification above; the perimeter gasket alone does not seal the screw penetrations. |
| Action-camera thumb screw / receiver | 1 set | M5, sufficient usable length for the actual mating stack. Receiver/nut is in the mating three-finger accessory. Verify head clearance opposite USB. |
| Transparent display window | Not specified | **Do not print in ASA or order from the placeholder dimensions.** Independent window dimensions and resistive-touch compatibility must be verified. A thick rigid cover can prevent resistive touch. |
| Powered USB weather boot / strain relief | Not specified | Requires the actual cable/plug. The model has a recessed bottom port and gasket channel, not a completed weatherproof live cable connection. |

## Geometry and component notes

- Body/bezel envelope is 100 × 70 × 49 mm excluding the bottom mount. Rear housing allowance is 45 mm including the 3 mm removable lid; front bezel is 4 mm.
- Front and rear enclosure screws share centres X = ±43, Y = ±29 mm. Insert pockets are blind and nominally 4.4 mm deep.
- The display PCB remains **85 × 48 mm**. Active LCD, glass/touch border and Z stack are separate, unverified parameters. The opening never derives from PCB size.
- The ESP32 cradle now faces the bottom USB port and clears the wall. Its 18 × 23.5 mm PCB outline comes from the supplied drawing. Actual PCB thickness, solder joints, wires, antenna and connector height still need physical checks.
- The bottom port centre is X = −27 mm on the −Y wall. A provisional 18 × 30 × 12 mm cable envelope is reserved. It has 3 mm lateral clearance from the mount root. This is not a measured plug envelope.
- Two mount fingers are 3 mm thick with one 3.3 mm gap. Nominal three-finger mating gauge has 0.15 mm side clearance at each interface. Actual accessory tolerance, pivot range and load strength need testing.
- Battery reservation is 52 × 36 × 12 mm; M8M assembly reservation is 30 × 30 × 14 mm. No exact battery, GPS breakout/antenna, SD-reader carrier or charging/protection circuit has been qualified. These are space allowances, not guaranteed-fit component selections.
- Display support ledges and tabs act on PCB edges. Verify that the real module has no components/pads in the contact regions; never clamp the glass.
- Keep the internal microSD assembly sealed inside; there is no external card slot in this model.

## Mandatory acceptance before the final order

1. Obtain/measure active LCD, glass/touch borders, PCB stack, connectors and support keepouts. Replace all applicable unverified display values.
2. Print the insert coupon in the intended ASA process. Select the correct pilot using the actual insert manufacturer's guidance and installation test.
3. Print/test the bezel, cradle and mount interfaces. Verify full pixel visibility, edge/corner touch, board retention, wired-pin clearance, USB insertion and mating mount motion.
4. Choose the final gasket material, cap/boot, screw heads and lengths. Verify gasket compression along the entire perimeter and all screw penetrations.
5. Check assembled thermal behaviour, fog/condensation, rain/road spray and sustained vibration/load. Only then approve final enclosure production and motorcycle installation.

`05_Verification/validation.md` records what was actually checked digitally. **A passed mesh/interference check does not replace these physical tests.** No IP rating, battery safety assessment or road-strength certification is claimed.

## Suggested supplier instruction

> Quote this revision as a fit-test prototype. Print rigid STLs in matte-black ASA using the table above; do not resize. Quote flexible files separately in TPU 85A for fit testing, or black silicone through a suitable process. Check quantities in filenames. Preserve seal faces and insert bores. Do not substitute rigid material for flexible seals. Contact the owner before changing dimensions, material, orientation or process.

## Assembly sequence

Test inserts first, then install them with controlled heat and perpendicular alignment. Fit the ESP32 into the removable cradle without trapping wires; verify USB alignment before closing the enclosure. Install the display onto rear PCB ledges and retain its PCB edges with the tabs. Fit the approved display seal/window arrangement. Install the joint gaskets and screw-head sealing washers, close the bezel/lid evenly, and confirm that screw tips do not bottom. Install the M5 fastener from the side opposite the USB cable. Validate cable strain relief and the complete assembly before use.
