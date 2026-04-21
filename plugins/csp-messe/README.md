# csp-messe

Ein CosmoScout-VR-Plugin für einen einfachen **Messe-Modus** mit

- globalem Hotkey `M`
- 30s Eingewöhnungsphase
- Flug-Timer
- "Nachladen" per `1 / 2 / 3 / 5`, aber nur unterhalb eines Restzeit-Schwellenwerts
- screen-space HUD mit animierter GIF (dadurch headset-/blickgebunden)

## Implementierte Logik

Die Beschreibung der Hotkey-Abfolge war nicht ganz eindeutig. Diese Umsetzung verhält sich deshalb so:

1. `M` aktiviert den Messe-Modus.
2. `1 / 2 / 3 / 5` startet die Eingewöhnungsphase und merkt sich eine Vorauswahl.
3. Nach der Eingewöhnung startet ein weiterer Druck auf `1 / 2 / 3 / 5` den eigentlichen Flug-Timer.
4. Während der aktiven Flugphase addieren `1 / 2 / 3 / 5` Zeit **nur dann**, wenn die Restzeit unter `addThresholdSeconds` liegt.
5. Optional kann per `autoStartAfterWarmup` eingestellt werden, dass die Zeit direkt nach der Eingewöhnung startet.

## Installation

Ordner nach `cosmoscout-vr/plugins/csp-messe` kopieren und CosmoScout VR normal kompilieren.

## Beispiel-Konfiguration

```json
{
  "plugins": {
    "csp-messe": {
      "showHudInIdle": true,
      "autoStartAfterWarmup": false,
      "warmupSeconds": 30.0,
      "addThresholdSeconds": 90.0,
      "hudGifPath": "../share/resources/gui/img/csp-messe-placeholder.gif",
      "hudWidth": 220,
      "hudHeight": 220
    }
  }
}
```

## Eigene GIF einbinden

Einfach `hudGifPath` auf eine andere Datei unterhalb von `share/resources/gui/...` zeigen lassen, z. B.

```json
"hudGifPath": "../share/resources/gui/img/mein-hud.gif"
```

## Wichtiger Hinweis zu Hotkeys

Das Plugin bindet `m`, `1`, `2`, `3` und `5` global über das ViSTA-Keyboard-System. Falls dein Branch diese Tasten bereits anders verwendet, musst du die Tasten oder das Binding anpassen.
