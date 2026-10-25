# Historical German question list

Archived 25 October 2026. Superseded by the current QUESTIONS.md after the first answers.
Original wording follows.

# Fragen für die erste Planung

Status aller Fragen: offen. Kurze Antworten mit Nummern genügen. „Empfehlung“ ist
eine mögliche Antwort; dann werden Alternativen mit Begründung vorgeschlagen.
Technische Detailfragen folgen nach Architekturspikes und Hardwareerhebung.

## Ziel und Umfang

1. Soll Aetheri zuerst ein Lern-/Forschungsprojekt, dein persönliches Alltagssystem
   oder ein OS für andere Menschen werden?
2. Welche drei bis fünf konkreten Dinge musst du damit in der ersten brauchbaren
   Version erledigen können?
3. Was hat Vorrang: möglichst viel eigener Code, frühe Alltagstauglichkeit,
   ausgefeilte UI, Geschwindigkeit oder breite Hardwareunterstützung?
4. Welchen Zeitrahmen und ungefähr welchen eigenen Zeitaufwand stellst du dir vor?
   Arbeitest du allein oder sollen weitere Menschen mitentwickeln?

## Was bedeutet selbst entwickelt?

5. Bedeutet „nur Linux als Basis“ wirklich eigene Implementierungen auch für
   C-Laufzeit, Systemprogramme, Grafik, Audio, TLS und Dateiformate? Oder sollen
   Desktop, Dienste und Apps selbst entstehen, mit ausgewählten Bibliotheken darunter?
6. Dürfen externe Compiler, Linker, Build-Werkzeuge, Debugger und VMs dauerhaft
   als Entwicklungswerkzeuge verwendet werden?
7. Sind vorübergehende Fremdkomponenten erlaubt, wenn ihr Ersatz ausdrücklich
   geplant und der ausgelieferte Bestand nachvollziehbar bleibt?
8. Sind Gerätefirmware und vorhandene Grafiktreiber im Userspace erlaubt?
   Der Kernel allein liefert beispielsweise nicht den ganzen Vulkan-/OpenGL-Stack.
9. Sollen Standards wie POSIX, Wayland und übliche Dateiformate unterstützt werden,
   auch wenn ihre Implementierungen selbst geschrieben werden?

## Hardware und Betrieb

10. Welcher Laptop steht bereit: Hersteller/Modell, CPU, GPU, RAM, SSD sowie
    möglichst WLAN-Chip? Seriennummern oder Zugangsdaten werden nicht benötigt.
11. Ist er ein freies Testgerät oder bleibt ein vorhandenes OS erhalten?
    Sind Live-USB, externe SSD oder Dual Boot gewünscht?
12. Reicht zunächst x86-64 mit UEFI oder müssen ARM64/ältere BIOS-Geräte früh mitlaufen?
13. Welche Hardwarefunktionen sind für die erste brauchbare Version Pflicht:
    WLAN, Bluetooth, Ton/Mikrofon, Kamera, Touchpad, externe Monitore, Suspend,
    Akkulaufzeit, Drucker oder Dockingstation?
14. Meinst du VMs auf dem jetzigen Entwicklungsrechner zum Testen von Aetheri,
    oder soll Aetheri selbst später auch VMs verwalten können?
15. Welche Grenzen gelten für VM-RAM und Speicherplatz? Vorschlag zur späteren
    Ausarbeitung: neue Abbilder und Build-Daten auf F:, keine großen Downloads auf D:.

## Desktop und Apps

16. Welche Oberflächen gefallen dir, und was daran genau? Welche Dinge möchtest du
    vermeiden? Beispiele können macOS, Windows, GNOME, KDE oder eigene Projekte sein.
17. Wie sollen Fenster und Navigation funktionieren: freie Fenster, Kacheln,
    Arbeitsflächen, Dock/Taskleiste, Startmenü, globale Suche, globale Menüleiste?
18. Welche visuelle Richtung möchtest du: ruhig/kompakt, Glas/Transparenz,
    helle/dunkle Themes, Akzentfarben und Animationen? Was soll Aetheri eigenständig machen?
19. Welche Sprachen, Tastaturlayouts und Eingaben müssen früh funktionieren?
    Welche Anforderungen gibt es an Skalierung, Tastaturbedienung und Screenreader?
20. Welche Apps haben Priorität? Kandidaten: Terminal, Dateien, Einstellungen,
    Browser, Texteditor, Systemmonitor, Rechner, Bild-/PDF-Betrachter, Medien,
    Kalender/Mail, Softwareverwaltung und Entwicklungswerkzeuge.
21. Soll zum Terminal auch eine eigene Shell mit eigener Befehlssprache entstehen?
    Müssen bestehende Shell-Skripte, SSH, Git, Vollbildprogramme oder Multiplexer laufen?
22. Bedeutet eigener Browser auch eigene HTML-/CSS-/JavaScript-Engine? Welche
    Webseiten und Funktionen sind Pflicht: Logins, Videos, Web-Apps, Downloads,
    Erweiterungen, Passwortverwaltung, WebAssembly oder DRM-Streaming?
23. Müssen bestehende Linux-Apps oder Windows-Programme/Spiele funktionieren?
    Welche konkret, und wären externe Kompatibilitätsschichten erlaubt?
24. Welche deiner Projekte sollen später auf Aetheri laufen oder Code beitragen?
    Neben NexaUI wurden lokal S9OS und Ultimate-Terminal gesehen, aber nicht untersucht.

## Technik und Pflege

25. Welche Programmiersprachen möchtest du verwenden oder lernen? Gibt es Vorgaben
    zu C, C++, Rust oder anderen Sprachen? Technische Auswahl kann ich begründet vorbereiten.
26. Soll NexaUI bei passender Eignung das gemeinsame App-Toolkit werden, oder
    möchtest du für Aetheri eine neue UI-Bibliothek? Darf notwendige NexaUI-Arbeit
    später als eigenes, abgestimmtes Arbeitspaket geplant werden?
27. Welche Sicherheitsfunktionen sind früh erforderlich: getrennte Benutzer,
    App-Berechtigungen/Sandboxen, Festplattenverschlüsselung, Secure Boot,
    Wiederherstellung und verlässliche Sicherungen?
28. Wie soll Software verteilt werden: Systemabbilder, einzelne Pakete,
    App-Store oder Kombination? Sind automatische Updates und Rückkehr zum vorherigen
    Stand gewünscht? Offline-Installation und langfristige Pflege mit einplanen?
29. Soll das System ohne Konto und offline voll nutzbar sein? Sind Cloud-Sync,
    NexaCloud-Anbindung oder optionale Diagnoseberichte überhaupt gewünscht?

## Veröffentlichung und Zusammenarbeit

30. Welche Lizenz möchtest du für den eigenen Code? Die Auswahl ist noch offen;
    Kernel und weitere übernommene Bestandteile behalten ihre eigenen Bedingungen.
31. Welche Sprache sollen README, technische Dokumentation und Benutzerhilfe haben:
    Deutsch, Englisch oder beides? Gibt es konkrete Schreibvorbilder?
32. Soll ich technische Detailentscheidungen innerhalb der vereinbarten Grenzen
    selbst vorbereiten und dokumentieren? Welche Produkt-/Designfragen möchtest du
    persönlich entscheiden, und gibt es ein Budget für CI, Domains oder Testhardware?

Öffentliche Veröffentlichung ist bereits beauftragt; vorgeschlagenes Ziel ist
`Lulus792/Aetheri`. Eine zusätzliche allgemeine Veröffentlichungsfreigabe ist
nicht erforderlich. Die erste Anforderungsrunde bleibt der nächste Schritt.
