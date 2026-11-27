import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: bar
    implicitHeight: 48
    color: Qt.alpha(theme.colors.darkBackground, 0.5)

    signal runClicked()
    signal explainClicked()
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

        Label {
            text: backend.open ? backend.databaseName : "Omasql"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: theme.colors.brightForeground
        }

        // Says at a glance that statements can't change this file.
        Rectangle {
            visible: backend.open && backend.databaseReadOnly
            implicitWidth: readOnlyLabel.implicitWidth + 12
            implicitHeight: 20
            radius: 4
            color: "transparent"
            border.color: theme.colors.yellow
            border.width: 1

            Label {
                id: readOnlyLabel
                anchors.centerIn: parent
                text: qsTr("read-only")
                color: theme.colors.yellow
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
        }

        Label {
            Layout.fillWidth: true
            text: backend.open && backend.databasePath !== ":memory:" ? backend.databasePath : (backend.open ? qsTr("in memory, gone when you close it") : "")
            elide: Text.ElideMiddle
            color: theme.colors.mutedForeground
            font.pixelSize: 12
        }

        BarButton {
            compact: bar.width < 1000
            visible: backend.open
            text: qsTr("History")
            hint: "Ctrl+H"
            onClicked: bar.historyClicked()
        }
        BarButton {
            compact: bar.width < 1000
            text: qsTr("Open")
            hint: "Ctrl+O"
            onClicked: backend.chooseDatabase()
        }
        BarButton {
            compact: bar.width < 1000
            visible: backend.open && bar.width > 900
            text: qsTr("Explain")
            hint: "Ctrl+⇧E"
            onClicked: bar.explainClicked()
        }
        BarButton {
            compact: bar.width < 1000
            visible: backend.open
            primary: true
            text: backend.running ? qsTr("Cancel") : qsTr("Run")
            hint: backend.running ? "Esc" : "Ctrl+↵"
            onClicked: backend.running ? backend.cancel() : bar.runClicked()
        }
        BarButton {
            compact: bar.width < 1000
            text: "?"
            hint: "F1"
            onClicked: bar.helpClicked()
        }
    }
}
