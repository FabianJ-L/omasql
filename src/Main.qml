import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1320
    height: 820
    minimumWidth: 820
    minimumHeight: 520
    visible: true
    title: backend.open ? backend.databaseName + " — Lode" : "Lode"
    color: theme.colors.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: theme.colors.background
    Material.foreground: theme.colors.foreground
    Material.primary: theme.colors.lighterBackground
    Material.roundedScale: theme.radius

    readonly property string mono: "monospace"
    property bool helpVisible: false
    property string pendingSql: ""

    // Statements that can drop data get a second look first.
    function requestRun(sql) {
        if (!backend.open || backend.running || sql.trim().length === 0)
            return
        if (backend.isDestructive(sql)) {
            pendingSql = sql
            confirm.open()
            return
        }
        backend.run(sql)
    }

    function handleEscape() {
        if (helpVisible) helpVisible = false
        else if (history.visible) history.close()
        else if (backend.running) backend.cancel()
        else if (backend.notice.length) backend.clearNotice()
        else if (results.hasSelection) results.clearSelection()
    }

    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: editor.runCurrent() }
    Shortcut { sequences: ["Ctrl+Shift+Return", "Ctrl+Shift+Enter"]; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: editor.runAll() }
    Shortcut { sequence: "Ctrl+Shift+E"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: editor.explainCurrent() }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; enabled: backend.hasResults; onActivated: results.focusFilter() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: win.handleEscape() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.chooseDatabase() }
    Shortcut { sequence: "Ctrl+W"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: { editor.saveNow(); backend.closeDatabase() } }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !(activeFocusItem instanceof TextInput) && !(activeFocusItem instanceof TextEdit); onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Ctrl+H"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: history.visible ? history.close() : history.open() }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: schema.focusFilter() }
    Shortcut { sequence: "Ctrl+R"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: backend.refreshSchema() }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: backend.hasResults; onActivated: backend.exportCsv() }
    Shortcut { sequence: "Ctrl+Shift+C"; context: Qt.ApplicationShortcut; enabled: backend.hasResults; onActivated: backend.copyResultsCsv() }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: schema.focusFilter() }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; enabled: backend.open; onActivated: editor.focusEditor() }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; enabled: backend.hasResults; onActivated: results.focusGrid() }

    onClosing: editor.saveNow()

    Connections {
        target: backend
        function onDatabaseChanged() { editor.load(backend.draft()) }
    }
    Component.onCompleted: if (backend.open) editor.load(backend.draft())

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TopBar {
                Layout.fillWidth: true
                onRunClicked: editor.runCurrent()
                onExplainClicked: editor.explainCurrent()
                onHelpClicked: win.helpVisible = !win.helpVisible
                onHistoryClicked: history.open()
            }

            WelcomeView {
                visible: !backend.open
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            SplitView {
                id: mainSplit
                visible: backend.open
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal
                handle: PaneDivider {}

                SchemaPanel {
                    id: schema
                    SplitView.preferredWidth: 270
                    SplitView.minimumWidth: 180
                    onInsertRequested: text => editor.insertText(text)
                }

                SplitView {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320
                    orientation: Qt.Vertical
                    handle: PaneDivider {}

                    SqlEditor {
                        id: editor
                        SplitView.preferredHeight: 260
                        SplitView.minimumHeight: 100
                    }

                    ResultsPane {
                        id: results
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 120
                    }
                }

                RowInspector {
                    visible: results.hasSelection
                    SplitView.preferredWidth: 340
                    SplitView.minimumWidth: 220
                    row: results.currentRow
                    column: results.currentColumn
                    onCloseRequested: results.clearSelection()
                }
            }
        }

        // Transient messages: open failures, exports, copies.
        Rectangle {
            id: noticeBar
            visible: backend.notice.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            width: Math.min(parent.width - 48, noticeText.implicitWidth + 48)
            height: noticeText.implicitHeight + 20
            radius: Math.min(6, theme.radius)
            color: theme.colors.lighterBackground
            border.color: theme.colors.border
            z: 50
            Text {
                id: noticeText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, noticeBar.parent.width - 96)
                text: backend.notice
                color: theme.colors.foreground
                wrapMode: Text.Wrap
                font.pixelSize: 13
            }
            TapHandler { onTapped: backend.clearNotice() }
            Timer {
                running: noticeBar.visible
                interval: 4000
                onTriggered: backend.clearNotice()
            }
        }

        DropArea {
            anchors.fill: parent
            onDropped: drop => {
                if (drop.hasUrls) {
                    editor.saveNow()
                    backend.openDatabase(drop.urls[0].toString())
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onCloseRequested: win.helpVisible = false
        }
    }

    HistoryPopup {
        id: history
        onPicked: sql => editor.replaceAll(sql)
    }

    Dialog {
        id: confirm
        anchors.centerIn: parent
        modal: true
        width: Math.min(560, win.width - 64)
        title: qsTr("Run this statement?")
        Material.roundedScale: theme.radius
        Material.background: theme.colors.lighterBackground
        standardButtons: Dialog.NoButton

        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("It can delete data or drop a table, and SQLite can't undo it once it's done.")
                color: theme.colors.lightForeground
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(160, pendingText.implicitHeight + 20)
                color: theme.colors.darkBackground
                radius: Math.min(4, theme.radius)
                clip: true
                Text {
                    id: pendingText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: win.pendingSql
                    font.family: win.mono
                    font.pixelSize: 13
                    color: theme.colors.red
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }
        }

        footer: DialogButtonBox {
            Material.background: "transparent"
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: qsTr("Run anyway")
                highlighted: true
                Material.accent: theme.colors.red
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: { backend.run(win.pendingSql); editor.focusEditor() }
        onRejected: editor.focusEditor()
    }
}
