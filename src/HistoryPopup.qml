import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Queries run before, newest first, filtered as you type. Enter puts the
// picked one into the editor.
Popup {
    id: popup
    signal picked(string sql)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent.width - 64)
    height: Math.min(560, parent.height - 64)
    modal: true
    focus: true
    padding: 0
    Material.roundedScale: theme.radius

    readonly property var entries: {
        const q = search.text.trim().toLowerCase()
        return backend.history.filter(h => q.length === 0 || h.sql.toLowerCase().includes(q))
    }

    function choose(index) {
        const entry = entries[index]
        if (!entry)
            return
        close()
        picked(entry.sql)
    }

    onOpened: {
        search.text = ""
        search.forceActiveFocus()
        list.currentIndex = 0
    }

    background: Rectangle {
        color: theme.colors.lighterBackground
        border.color: theme.colors.border
        radius: Math.min(8, theme.radius)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        FilterField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 12
            placeholder: qsTr("Search history")
            onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
            onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
            onAccepted: popup.choose(list.currentIndex)
            onEscaped: popup.close()
            onTextChanged: list.currentIndex = 0
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: popup.entries
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: entry
                required property var modelData
                required property int index
                width: ListView.view.width
                height: Math.min(sqlText.implicitHeight, 3 * 18) + 30
                color: ListView.isCurrentItem ? theme.colors.selection : hover.hovered ? Qt.alpha(theme.colors.selection, 0.4) : "transparent"

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    anchors.leftMargin: 14
                    spacing: 3
                    Text {
                        id: sqlText
                        width: parent.width
                        height: Math.min(implicitHeight, 3 * 18)
                        text: entry.modelData.sql
                        color: theme.colors.foreground
                        font.family: win.mono
                        font.pixelSize: 13
                        clip: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: Qt.formatDateTime(new Date(entry.modelData.at), Qt.locale(translator.language).dateTimeFormat(Locale.ShortFormat))
                              + (entry.modelData.database ? "  ·  " + entry.modelData.database : "")
                        color: theme.colors.mutedForeground
                        font.pixelSize: 11
                    }
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: popup.choose(entry.index) }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: backend.history.length ? qsTr("Nothing matches") : qsTr("Queries you run show up here")
                color: theme.colors.mutedForeground
            }
        }
    }
}
