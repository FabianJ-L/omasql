import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQml.Models

// The last query's rows: a status line, then a grid with typed column
// headers, row numbers, and click-to-sort.
Rectangle {
    id: pane
    color: theme.colors.background

    readonly property int currentRow: table.currentRow
    readonly property int currentColumn: table.currentColumn
    readonly property bool hasSelection: backend.hasResults && table.currentRow >= 0

    function clearSelection() {
        selection.clear()
        selection.clearCurrentIndex()
    }
    function focusGrid() {
        if (table.currentRow < 0 && table.rows > 0)
            selection.setCurrentIndex(backend.results.index(0, 0), ItemSelectionModel.ClearAndSelect)
        table.forceActiveFocus()
    }

    readonly property real charWidth: metrics.averageCharacterWidth
    FontMetrics { id: metrics; font.family: win.mono; font.pixelSize: 13 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: theme.colors.darkBackground

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.colors.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 12

                BusyIndicator {
                    visible: backend.running
                    running: visible
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                Text {
                    text: backend.status
                    color: ({ "error": theme.colors.red, "warning": theme.colors.yellow, "ok": theme.accent })[backend.statusKind]
                           || theme.colors.lightForeground
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.maximumWidth: pane.width * 0.6
                }
                Text {
                    Layout.fillWidth: true
                    text: backend.lastSql.replace(/\s+/g, " ")
                    color: theme.colors.mutedForeground
                    font.family: win.mono
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                BarButton {
                    visible: backend.hasResults
                    text: qsTr("Copy CSV")
                    hint: "Ctrl+Shift+C"
                    implicitHeight: 24
                    onClicked: backend.copyResultsCsv()
                }
                BarButton {
                    visible: backend.hasResults
                    text: qsTr("Export…")
                    hint: "Ctrl+E"
                    implicitHeight: 24
                    onClicked: backend.exportCsv()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            HorizontalHeaderView {
                id: header
                syncView: table
                anchors.left: table.left
                anchors.top: parent.top
                clip: true
                visible: backend.hasResults
                delegate: Rectangle {
                    id: head
                    required property int column
                    implicitHeight: 44
                    implicitWidth: 80
                    color: theme.colors.lighterBackground
                    readonly property bool sorted: backend.results.sortColumn === column

                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: theme.colors.border }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.colors.border }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 22
                        spacing: 1
                        Text {
                            width: parent.width
                            text: backend.results.revision, backend.results.columnName(head.column)
                            color: head.column === table.currentColumn ? theme.accent : theme.colors.brightForeground
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: backend.results.revision, (backend.results.columnType(head.column) || "—").toLowerCase()
                            color: theme.colors.mutedForeground
                            font.family: win.mono
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: head.sorted
                        text: backend.results.sortDescending ? "↓" : "↑"
                        color: theme.accent
                        font.pixelSize: 13
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: backend.results.sortBy(head.column) }
                    ToolTip.visible: headHover.hovered
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Sort by this column")
                    HoverHandler { id: headHover }
                }
            }

            VerticalHeaderView {
                id: rowHeader
                syncView: table
                anchors.top: table.top
                anchors.left: parent.left
                clip: true
                visible: backend.hasResults
                delegate: Rectangle {
                    required property int row
                    implicitWidth: Math.max(44, rowNumbers.advanceWidth(String(backend.results.rows)) + 20)
                    implicitHeight: 28
                    color: row === table.currentRow ? theme.colors.selection : theme.colors.darkBackground
                    Text {
                        anchors.fill: parent
                        anchors.rightMargin: 8
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        text: parent.row + 1
                        color: theme.colors.mutedForeground
                        font.family: win.mono
                        font.pixelSize: 11
                    }
                    TapHandler { onTapped: selection.setCurrentIndex(backend.results.index(parent.row, Math.max(0, table.currentColumn)), ItemSelectionModel.ClearAndSelect) }
                }
                FontMetrics { id: rowNumbers; font.family: win.mono; font.pixelSize: 11 }
            }

            TableView {
                id: table
                anchors.left: rowHeader.right
                anchors.top: header.bottom
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: backend.hasResults
                clip: true
                model: backend.results
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                pointerNavigationEnabled: true
                selectionBehavior: TableView.SelectCells
                selectionModel: ItemSelectionModel { id: selection; model: backend.results }
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar {}

                columnWidthProvider: column => {
                    backend.results.revision
                    const chars = backend.results.columnChars(column)
                    return Math.max(70, Math.min(440, chars * pane.charWidth + 34))
                }

                Connections {
                    target: backend.results
                    function onChanged() {
                        table.forceLayout()
                        table.positionViewAtRow(0, TableView.AlignTop)
                    }
                }

                Keys.onPressed: event => {
                    if (event.matches(StandardKey.Copy) && table.currentRow >= 0) {
                        backend.copy(backend.results.cellText(table.currentRow, table.currentColumn))
                        event.accepted = true
                    }
                }

                delegate: Rectangle {
                    id: cell
                    required property string display
                    required property string kind
                    required property bool current
                    required property int row
                    implicitHeight: 28
                    color: current ? theme.colors.selection
                         : row === table.currentRow ? Qt.alpha(theme.colors.selection, 0.45)
                         : row % 2 ? theme.colors.background : Qt.lighter(theme.colors.background, 1.06)
                    border.width: current ? 1 : 0
                    border.color: theme.accent

                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Qt.alpha(theme.colors.border, 0.5) }

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: cell.kind === "integer" || cell.kind === "real" ? Text.AlignRight : Text.AlignLeft
                        text: cell.display
                        elide: Text.ElideRight
                        font.family: win.mono
                        font.pixelSize: 13
                        font.italic: cell.kind === "null"
                        color: ({
                            "null": theme.colors.mutedForeground, "integer": theme.colors.orange,
                            "real": theme.colors.orange, "blob": theme.colors.magenta
                        })[cell.kind] || theme.colors.foreground
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                visible: !backend.hasResults
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: backend.running ? qsTr("Running…")
                        : backend.statusKind === "error" ? qsTr("The query failed")
                        : backend.status.length ? qsTr("No rows to show")
                        : qsTr("Run a query or click a table")
                    color: theme.colors.lightForeground
                    font.pixelSize: 15
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !backend.running && backend.status.length === 0
                    text: qsTr("Ctrl+Enter runs the statement under the cursor")
                    color: theme.colors.mutedForeground
                    font.pixelSize: 12
                }
            }
        }
    }
}
