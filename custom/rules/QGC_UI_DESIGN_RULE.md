# QGroundControl Custom UI Design System Rule

> **MANDATORY FOR ALL QML CODE IN THIS REPOSITORY**:
> Every custom page, tab, dialog, and control must strictly follow upstream QGroundControl styling principles (as exemplified by standard pages such as MAVLink 2.0 Logging and AppSettings).
> Violations (such as raw hex color codes or ad-hoc custom Rectangle buttons) will be rejected.

---

## 1. Palette & Colors (Zero Raw Hex Rule)

- **NEVER** hardcode hex colors (e.g. `#1a1a1a`, `#2ECC71`, `#E74C3C`, `#7F8C8D`, `#222`, `#ffffff`).
- **ALWAYS** declare a local `QGCPalette { id: qgcPal }` and bind to semantic palette properties:
  - Background surface: `qgcPal.window`
  - Card / Group surface: `qgcPal.windowShade`
  - Nested container / Input background: `qgcPal.windowShadeDark`
  - Body text: `qgcPal.text`
  - Button text: `qgcPal.buttonText`
  - Button surface: `qgcPal.button`
  - Accent / Selection / Highlight: `qgcPal.buttonHighlight`
  - Status Success / Active: `qgcPal.colorGreen`
  - Status Danger / Error / Offline: `qgcPal.colorRed`
  - Status Warning / In-Progress: `qgcPal.colorOrange`
  - Subtle text / Disabled: `qgcPal.text` with `opacity: 0.6`

---

## 2. Controls & Widgets (Upstream Native Controls Only)

Do not reinvent standard controls using basic `Rectangle` and `MouseArea`. Always import `QGroundControl.Controls` and use:
- **Labels**: `QGCLabel` (automatically respects QGC font family, anti-aliasing, and themes).
- **Buttons**: `QGCButton` (automatically handles hover, pressed states, focus, and border radius).
- **Dropdowns**: `QGCComboBox` (with `sizeToContents: true`).
- **Inputs**: `QGCTextField` (standard padding, border color, and cursor styling).
- **Toggles**: `QGCSwitch` or `QGCCheckBox`.
- **Card Containers**: `SettingsGroupLayout` with `heading: qsTr("...")`.

---

## 3. Typography & Sizing

- **Font Family**: Standard default is set by `QGroundControl.corePlugin.defaultFont`. Monospace logs use `ScreenTools.fixedFontFamily`.
- **Point Sizes**: Use semantic ScreenTools properties:
  - Heading / Title: `ScreenTools.largeFontPointSize`
  - Body text: `ScreenTools.defaultFontPointSize`
  - Metrics / Footnotes / Captions: `ScreenTools.smallFontPointSize`
- **Spacing & Padding**:
  - Horizontal spacing: `ScreenTools.defaultFontPixelWidth * 1.5`
  - Vertical spacing / Margins: `ScreenTools.defaultFontPixelHeight * 0.5`
  - Standard button width: `ScreenTools.defaultFontPixelWidth * 12`

---

## 4. Two-Column Settings Layout Pattern (MAVLink 2.0 Style)

For all configuration rows:
```qml
RowLayout {
    Layout.fillWidth: true
    spacing: ScreenTools.defaultFontPixelWidth * 2

    QGCLabel {
        text: qsTr("Setting Label:")
        Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 24
        color: qgcPal.text
    }

    QGCComboBox {
        Layout.fillWidth: true
        model: ["Option 1", "Option 2"]
    }
}
```
