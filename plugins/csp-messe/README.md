# csp-messe

CosmoScout-VR-Plugin für einen Messe-Modus mit:

- Hotkey `M` zum Starten des Messe-Modus
- 30s Eingewöhnungsphase
- Flug-Timer mit `1 / 2 / 3 / 5`
- Zeitaddition nur unterhalb eines Restzeit-Schwellenwerts
- VR-HUD als eigenes `WorldSpaceGuiArea`-Panel statt als normales CosmoScout-Menüelement

## Verhalten

1. `M` aktiviert den Messe-Modus.
2. `1 / 2 / 3 / 5` startet die Eingewöhnungsphase und merkt sich die Vorauswahl.
3. Nach der Eingewöhnung startet ein weiterer Druck auf `1 / 2 / 3 / 5` den Flug-Timer.
4. Während des Flugs addieren `1 / 2 / 3 / 5` nur dann Zeit, wenn die Restzeit unter `addThresholdSeconds` liegt.
5. Optional kann `autoStartAfterWarmup` die Vorauswahl direkt nach der Eingewöhnung starten.

## Was an dieser Version anders ist

Diese Fassung hängt das HUD **nicht** mehr als HTML-Box an die normale CosmoScout-Oberfläche. Stattdessen wird ein eigenes GUI-Panel im 3D-Raum erzeugt und in jedem Frame vor den Beobachter gesetzt.

## Beispiel-Konfiguration

```json
{
  "plugins": {
    "csp-messe": {
      "showHudInIdle": true,
      "autoStartAfterWarmup": true,
      "warmupSeconds": 0.01,
      "addThresholdSeconds": 45.0,
      "hudGifPath": "../share/resources/gui/img/csp-messe.webp",
      "hudWidth": 4800,
      "hudHeight": 2500,
      "hudDistanceMeters": 3.0,
      "hudVerticalOffsetMeters": 1.25,
      "hudScale": 2.50,
      "heightScale": 3.0,
      "ignoreDepth": false
    },
  }
}
```

## Hinweise

- Das HUD wird jedes Frame vor den Beobachter gesetzt.
- Wenn das Panel zu nah, zu weit, zu hoch oder zu tief sitzt, passe `hudDistanceMeters` und `hudVerticalOffsetMeters` an.
- Wenn du eine eigene GIF verwendest, gib am besten direkt einen `file://`-Pfad unter `share/resources/gui/...` an.
