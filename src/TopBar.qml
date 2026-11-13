import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: bar
    implicitHeight: 48
    color: theme.colors.darkBackground

    signal runClicked()
    signal helpClicked()
    signal historyClicked()

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: theme.colors.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 10
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            radius: 5
            color: backend.running ? theme.colors.yellow : backend.open ? theme.accent : theme.colors.mutedForeground
        }

        Label {
            text: backend.open ? backend.databaseName : "Lode"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: theme.colors.brightForeground
        }

        Label {
            Layout.fillWidth: true
            text: backend.open && backend.databasePath !== ":memory:" ? backend.databasePath : (backend.open ? qsTr("in memory, gone when you close it") : "")
            elide: Text.ElideMiddle
            color: theme.colors.mutedForeground
            font.pixelSize: 12
        }

        BarButton {
            visible: backend.open
            text: qsTr("History")
            hint: "Ctrl+H"
            onClicked: bar.historyClicked()
        }
        BarButton {
            text: qsTr("Open")
            hint: "Ctrl+O"
            onClicked: backend.chooseDatabase()
        }
        BarButton {
            visible: backend.open
            primary: true
            text: backend.running ? qsTr("Cancel") : qsTr("Run")
            hint: backend.running ? "Esc" : "Ctrl+↵"
            onClicked: backend.running ? backend.cancel() : bar.runClicked()
        }
        BarButton {
            text: "?"
            hint: "F1"
            onClicked: bar.helpClicked()
        }
    }
}
