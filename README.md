# C4D QuadDraw — Retopology Tool for Cinema 4D (2026)

An interactive retopology tool for Cinema 4D inspired by Autodesk Maya's **Quad Draw**. It allows artists to quickly create, edit, and relax quad-based geometry over an underlying reference surface or in freeform mode.

---

## Features

1. **Editable Mesh and Target Assignment**:
   - Activating the tool with a selected polygon object designates it as the **editable mesh**.
   - A custom **QuadDraw** tag (`Tquaddraw`) is automatically assigned to the editable mesh if not already present.
   - The tag provides a **Target Mesh** link field where you can drop any reference object to enable surface snapping.
   - **No target specified**: the mesh can be edited in freeform mode without snapping (vertices and newly placed points move in camera view plane space).
   - **No selection**: activating the tool without an active mesh automatically creates a new `QuadDraw_Retopo` object with Phong and QuadDraw tags.

2. **Surface Snapping**:
   - Continuous raycast projection onto the target mesh surface (`TargetSurface`).
   - Screen-space snapping to existing retopo vertices with automatic weld on drop.

3. **Quad Creation**:
   - **Shift + Hover**: previews prospective quad polygons based on nearby vertices.
   - **Shift + LMB**: instantly creates quad polygons with correct surface normal orientation.

4. **Maya-Style Relax Brush**:
   - **Shift + LMB Drag** over faces or edges smooths topology.
   - Multiple relax modes: Auto-Lock (preserves borders or interiors depending on stroke origin), Border Only, Interior Only, or All.
   - **Shift + MMB Drag**: interactively adjusts relax brush radius.

5. **Edge Loop Insertion (Cut Tool)**:
   - **Ctrl + Hover**: previews edge loops across quad rings.
   - **Ctrl + LMB Drag**: inserts edge loops with interactive slide positioning.

6. **Interactive Component Deletion**:
   - **Ctrl + Shift + Hover**: highlights vertices, edge loops, or polygons in red.
   - **Ctrl + Shift + LMB**: removes the highlighted component.

7. **Tweak Mode**:
   - **LMB Drag** on vertices, edges, or polygons moves components directly.
   - Dragging a vertex onto another merges them automatically (Weld).

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| **LMB Click** | Place a point on the surface (or in view plane if no target) |
| **LMB Drag** | Tweak/move vertex, edge, or polygon (welds on vertex drop) |
| **Shift + Hover** | Preview prospective quad polygon |
| **Shift + LMB** | Create quad polygon from preview |
| **Shift + LMB Drag** | Relax topology using the relax brush |
| **Shift + MMB Drag** | Interactively resize relax brush radius |
| **Ctrl + Hover** | Preview edge loop cut |
| **Ctrl + LMB / Drag** | Insert edge loop and slide |
| **Ctrl + Shift + Hover** | Highlight vertex, edge loop, or polygon for deletion |
| **Ctrl + Shift + LMB** | Delete highlighted element |
| **Esc** | Cancel active preview or slide operation |

---

## Build and Installation

- **Cinema 4D 2026**:
  - Build: [build_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.bat) or [build_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2026.ps1)
  - Deploy: [deploy_2026.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.bat) or [deploy_2026.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2026.ps1)

- **Cinema 4D 2025**:
  - Build: [build_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.bat) or [build_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/build_2025.ps1)
  - Deploy: [deploy_2025.bat](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.bat) or [deploy_2025.ps1](file:///c:/Users/user/Desktop/cpp/C4D_QuadDraw/deploy_2025.ps1)
