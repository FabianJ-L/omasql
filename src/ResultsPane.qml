import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQml.Models

// The last query's rows: a status line with a filter, then a grid with typed
// column headers, row numbers, click-to-sort, and columns you can drag wider.
Rectangle {
    id: pane
    color: "transparent"

    readonly property int currentRow: table.currentRow
    readonly property int currentColumn: table.currentColumn
    readonly property bool hasSelection: backend.hasResults && table.currentRow >= 0
    // The failed statement came from the editor and can go to the AI.
    property bool canFix: false
    signal fixRequested()

    function clearSelection() {
        selection.clear()
        selection.clearCurrentIndex()
    }
    function focusFilter() {
        filter.forceActiveFocus()
        filter.selectAll()
    }
    // The selected block of cells (Shift+click or Shift+arrows), or the
    // current cell, as text to paste into a spreadsheet.
    function copySelection() {
        const indexes = selection.selectedIndexes
        if (indexes.length > 1) {
            let top = Infinity, left = Infinity, bottom = -1, right = -1
            for (const i of indexes) {
                top = Math.min(top, i.row); bottom = Math.max(bottom, i.row)
                left = Math.min(left, i.column); right = Math.max(right, i.column)
            }
            backend.copy(backend.results.rangeText(top, left, bottom, right))
        } else if (table.currentRow >= 0) {
            backend.copy(backend.results.cellText(table.currentRow, table.currentColumn))
        }
    }
    // Per column: the "table.column" it points to when it is a foreign key of
    // the table the rows come from, else "".
    readonly property var references: {
        backend.results.revision; backend.resultTable
        const out = []
        for (let c = 0; c < backend.results.columns; ++c)
            out.push(backend.referenceOf(backend.results.columnName(c)))
        return out
    }
    readonly property var formats: [
        { id: "csv", label: "CSV" },
        { id: "json", label: "JSON" },
        { id: "markdown", label: qsTr("Markdown table") },
        { id: "sql", label: qsTr("SQL INSERT statements") }
    ]

    function focusGrid() {
        if (table.currentRow < 0 && table.rows > 0)
            selection.setCurrentIndex(backend.results.index(0, 0), ItemSelectionModel.ClearAndSelect)
        table.forceActiveFocus()
    }

    readonly property real charWidth: metrics.averageCharacterWidth
    // Below this the toolbar drops the shortcut hints from its buttons.
    readonly property bool narrow: pane.width < 950
    FontMetrics { id: metrics; font.family: win.mono; font.pixelSize: 13 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Qt.alpha(theme.colors.darkBackground, 0.5)

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.colors.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: pane.narrow ? 8 : 12

                BusyIndicator {
                    visible: backend.running || ai.busy
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
                    // Gives way first when the bar is short of room; the
                    // editor still marks where the error is.
                    Layout.fillWidth: pane.narrow
                    Layout.minimumWidth: Math.min(implicitWidth, 60)
                    Layout.preferredWidth: implicitWidth
                    Layout.maximumWidth: Math.min(implicitWidth, pane.width * (fixButton.visible || moreButton.visible ? 0.45 : 0.6))
                }
                BarButton {
                    id: moreButton
                    compact: pane.narrow
                    visible: backend.canLoadMore && !backend.running
                    text: qsTr("Load %1 more").arg(backend.rowBatch.toLocaleString(Qt.locale(translator.language), 'f', 0))
                    implicitHeight: 24
                    onClicked: backend.loadMore()
                }
                BarButton {
                    id: fixButton
                    compact: pane.narrow
                    visible: ai.available && (ai.busy || (pane.canFix && backend.statusKind === "error"))
                    primary: !ai.busy
                    text: ai.busy ? qsTr("%1 is fixing it…").arg(ai.agentLabel) : qsTr("Fix with %1").arg(ai.agentLabel)
                    hint: ai.busy ? "Esc" : "Ctrl+."
                    implicitHeight: 24
                    onClicked: ai.busy ? ai.cancel() : pane.fixRequested()
                }
                // Which agent fixes it: Claude, Codex, or any other one installed.
                BarButton {
                    id: agentButton
                    visible: fixButton.visible && !ai.busy && ai.agents.length > 1
                    text: "▾"
                    implicitHeight: 24
                    onClicked: agentMenu.popup(agentButton, 0, agentButton.height + 2)
                    ToolTip.visible: agentHover.hovered
                    ToolTip.text: qsTr("Choose the AI agent")
                    ToolTip.delay: 400
                    HoverHandler { id: agentHover }
                    Menu {
                        id: agentMenu
                        Material.roundedScale: theme.radius
                        Repeater {
                            model: ai.agents
                            MenuItem {
                                required property string modelData
                                text: ai.answersInBackground(modelData) ? ai.label(modelData)
                                                                         : qsTr("%1 (in a terminal)").arg(ai.label(modelData))
                                checkable: true
                                checked: modelData === ai.agent
                                onTriggered: ai.setAgent(modelData)
                            }
                        }
                    }
                }
                Text {
                    // Only takes the room left over.
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    text: backend.lastSql.replace(/\s+/g, " ")
                    color: theme.colors.mutedForeground
                    font.family: win.mono
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                Text {
                    visible: backend.hasResults && backend.results.filter.length > 0
                    text: qsTr("%1 of %2").arg(backend.results.rows.toLocaleString(Qt.locale(translator.language), 'f', 0))
                                         .arg(backend.results.totalRows.toLocaleString(Qt.locale(translator.language), 'f', 0))
                    color: theme.colors.mutedForeground
                    font.pixelSize: 12
                }
                FilterField {
                    id: filter
                    visible: backend.hasResults
                    Layout.fillWidth: true
                    Layout.minimumWidth: 90
                    Layout.maximumWidth: 240
                    Layout.preferredWidth: Math.min(240, pane.width * 0.25)
                    implicitHeight: 24
                    placeholder: qsTr("Filter rows")
                    hint: "Ctrl+F"
                    onTextChanged: filterDelay.restart()
                    onEscaped: { text = ""; backend.results.filter = ""; table.forceActiveFocus() }
                    onAccepted: { backend.results.filter = text; pane.focusGrid() }
                    onDownPressed: pane.focusGrid()
                    Timer {
                        id: filterDelay
                        interval: 150
                        onTriggered: backend.results.filter = filter.text
                    }
                    Connections {
                        target: backend.results
                        function onChanged() { if (backend.results.filter.length === 0 && filter.text.length > 0 && !filterDelay.running) filter.text = "" }
                    }
                }
                BarButton {
                    id: copyButton
                    compact: pane.narrow
                    visible: backend.hasResults
                    text: qsTr("Copy") + " ▾"
                    hint: "Ctrl+Shift+C"
                    implicitHeight: 24
                    onClicked: copyMenu.popup(copyButton, 0, copyButton.height + 2)
                    Menu {
                        id: copyMenu
                        Material.roundedScale: theme.radius
                        Repeater {
                            model: pane.formats
                            MenuItem {
                                required property var modelData
                                text: modelData.label
                                onTriggered: backend.copyResults(modelData.id)
                            }
                        }
                    }
                }
                BarButton {
                    id: exportButton
                    compact: pane.narrow
                    visible: backend.hasResults
                    text: qsTr("Export") + " ▾"
                    hint: "Ctrl+E"
                    implicitHeight: 24
                    onClicked: exportMenu.popup(exportButton, 0, exportButton.height + 2)
                    Menu {
                        id: exportMenu
                        Material.roundedScale: theme.radius
                        Repeater {
                            model: pane.formats
                            MenuItem {
                                required property var modelData
                                text: modelData.label + "…"
                                onTriggered: backend.exportResults(modelData.id)
                            }
                        }
                    }
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
                // Drag a header's right edge too, not only the grid's.
                resizableColumns: true
                visible: backend.hasResults
                delegate: Rectangle {
                    id: head
                    required property int column
                    implicitHeight: 44
                    implicitWidth: 80
                    color: Qt.alpha(theme.colors.lighterBackground, 0.7)
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
                    color: row === table.currentRow ? theme.colors.selection : Qt.alpha(theme.colors.darkBackground, 0.5)
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
                resizableColumns: true
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                pointerNavigationEnabled: true
                selectionBehavior: TableView.SelectCells
                selectionModel: ItemSelectionModel { id: selection; model: backend.results }
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar {}

                columnWidthProvider: column => {
                    backend.results.revision
                    // A width someone dragged wins over the fitted one.
                    const dragged = explicitColumnWidth(column)
                    if (dragged >= 0)
                        return Math.max(40, dragged)
                    const chars = backend.results.columnChars(column)
                    return Math.max(70, Math.min(440, chars * pane.charWidth + 34))
                }

                // A new result starts with fitted widths again; a filter or a
                // sort keeps the widths someone dragged.
                property int shownRevision: -1
                Connections {
                    target: backend.results
                    function onChanged() {
                        if (table.shownRevision !== backend.results.revision) {
                            table.shownRevision = backend.results.revision
                            table.clearColumnWidths()
                        }
                        pane.clearSelection()
                        table.forceLayout()
                        if (table.rows > 0)
                            table.positionViewAtRow(0, TableView.AlignTop)
                    }
                }
                Connections {
                    target: backend
                    function onMoreLoaded(first) {
                        // Pick up where the rows ended, unless a sort or a
                        // filter moved them around.
                        if (backend.results.sortColumn < 0 && backend.results.filter.length === 0 && first < table.rows)
                            table.positionViewAtRow(first, TableView.AlignTop)
                    }
                }

                Keys.onPressed: event => {
                    if (event.matches(StandardKey.Copy)) {
                        pane.copySelection()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (table.currentRow >= 0 && backend.referenceOf(backend.results.columnName(table.currentColumn)).length)
                            backend.followReference(table.currentRow, table.currentColumn)
                        event.accepted = true
                    }
                }

                delegate: Rectangle {
                    id: cell
                    required property string display
                    required property string kind
                    required property bool current
                    required property bool selected
                    required property int row
                    required property int column
                    readonly property string reference: pane.references[column] || ""
                    readonly property bool link: reference.length > 0 && kind !== "null"
                    implicitHeight: 28
                    color: current || selected ? theme.colors.selection
                         : row === table.currentRow ? Qt.alpha(theme.colors.selection, 0.45)
                         : row % 2 ? "transparent" : Qt.alpha(theme.colors.foreground, 0.03)
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
                        font.underline: cell.link && linkHover.hovered
                        color: cell.link ? theme.colors.blue : ({
                            "null": theme.colors.mutedForeground, "integer": theme.colors.orange,
                            "real": theme.colors.orange, "blob": theme.colors.magenta
                        })[cell.kind] || theme.colors.foreground
                    }
                    HoverHandler {
                        id: linkHover
                        enabled: cell.link
                        acceptedModifiers: Qt.ControlModifier
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        enabled: cell.link
                        acceptedModifiers: Qt.ControlModifier
                        onTapped: backend.followReference(cell.row, cell.column)
                    }
                    ToolTip.visible: cell.link && cellHover.hovered
                    ToolTip.delay: 900
                    ToolTip.text: qsTr("Ctrl+click or Enter opens %1").arg(cell.reference)
                    HoverHandler { id: cellHover; enabled: cell.link }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: backend.hasResults && backend.results.rows === 0 && backend.results.filter.length > 0
                text: qsTr("No rows match “%1”").arg(backend.results.filter)
                color: theme.colors.lightForeground
                font.pixelSize: 15
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
