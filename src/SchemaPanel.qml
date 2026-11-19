import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The database's tables and views with their columns, keys, and row counts.
// Clicking a table previews its rows and opens it; the arrow opens and closes
// it. Clicking a column inserts its name.
Rectangle {
    id: panel
    color: theme.colors.darkBackground

    signal insertRequested(string text)

    // Table name -> true (open) or false (closed). A table nobody touched is
    // open while the filter matches one of its columns.
    property var expanded: ({})
    readonly property string query: filter.text.trim().toLowerCase()
    readonly property var shown: {
        const q = query
        return backend.tables.map(t => {
            const nameMatch = q.length === 0 || t.name.toLowerCase().includes(q)
            const columnMatch = q.length > 0 && t.columns.some(c => c.name.toLowerCase().includes(q))
            return { table: t, visible: nameMatch || columnMatch, columnMatch: columnMatch && !nameMatch }
        }).filter(e => e.visible)
    }

    function focusFilter() {
        filter.forceActiveFocus()
        filter.selectAll()
    }
    function isOpen(name, columnMatch) {
        return name in expanded ? expanded[name] : columnMatch
    }
    function setOpen(name, open) {
        const next = Object.assign({}, expanded)
        next[name] = open
        expanded = next
    }
    function collapseAll() {
        const next = {}
        for (const t of backend.tables)
            next[t.name] = false
        expanded = next
    }
    readonly property bool anyOpen: shown.some(e => isOpen(e.table.name, e.columnMatch))
    function formatCount(n) {
        return Number(n).toLocaleString(Qt.locale(translator.language), 'f', 0)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 6
            Layout.topMargin: 8
            Label {
                Layout.fillWidth: true
                text: qsTr("Tables")
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                color: theme.colors.mutedForeground
            }
            Label {
                text: backend.tables.length
                font.pixelSize: 12
                color: theme.colors.mutedForeground
            }
            ToolButton {
                text: "⊟"
                font.pixelSize: 15
                enabled: panel.anyOpen
                Material.foreground: theme.colors.mutedForeground
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Collapse all")
                onClicked: panel.collapseAll()
            }
            ToolButton {
                text: "↻"
                font.pixelSize: 15
                Material.foreground: theme.colors.mutedForeground
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reload schema (Ctrl+R)")
                onClicked: backend.refreshSchema()
            }
        }

        FilterField {
            id: filter
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 8
            placeholder: qsTr("Filter tables and columns")
            hint: "Ctrl+K"
            onEscaped: { text = ""; panel.insertRequested("") }
            onAccepted: if (panel.shown.length > 0) backend.preview(panel.shown[0].table.name)
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: panel.shown
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: tableEntry
                required property var modelData
                readonly property var table: modelData.table
                readonly property bool open: panel.isOpen(table.name, modelData.columnMatch)
                readonly property bool active: backend.resultTable === table.name
                width: ListView.view.width

                Rectangle {
                    id: tableRow
                    width: parent.width
                    height: 32
                    color: tableEntry.active ? Qt.alpha(theme.colors.selection, 0.8)
                         : rowHover.hovered ? theme.colors.lighterBackground : "transparent"

                    Rectangle {
                        visible: tableEntry.active
                        width: 2
                        height: parent.height
                        color: theme.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 12
                        spacing: 4

                        Text {
                            id: arrow
                            Layout.preferredWidth: 18
                            horizontalAlignment: Text.AlignHCenter
                            text: tableEntry.open ? "▾" : "▸"
                            color: arrowHover.hovered ? theme.colors.foreground : theme.colors.mutedForeground
                            font.pixelSize: 16
                            HoverHandler { id: arrowHover }
                        }
                        Text {
                            text: tableEntry.table.kind === "view" ? "◇" : "▦"
                            color: tableEntry.table.kind === "view" ? theme.colors.magenta : theme.colors.yellow
                            font.pixelSize: 13
                        }
                        Text {
                            Layout.fillWidth: true
                            text: tableEntry.table.name
                            color: theme.colors.foreground
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Text {
                            text: tableEntry.table.kind === "view" ? qsTr("view") : panel.formatCount(tableEntry.table.rows)
                            color: theme.colors.mutedForeground
                            font.pixelSize: 11
                            font.family: win.mono
                        }
                    }

                    HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                    // One handler for the whole row, so a click on the arrow
                    // can't both close the table and open it again.
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: (point, button) => {
                            const name = tableEntry.table.name
                            const onArrow = arrow.mapFromItem(tableRow, point.position)
                            if (onArrow.x >= -6 && onArrow.x <= arrow.width + 6) {
                                panel.setOpen(name, !tableEntry.open)
                            } else {
                                if (!tableEntry.open)
                                    panel.setOpen(name, true)
                                backend.preview(name)
                            }
                        }
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: tableMenu.popup()
                    }
                    ToolTip.visible: rowHover.hovered && tableEntry.table.sql.length > 0
                    ToolTip.delay: 900
                    ToolTip.text: tableEntry.table.sql

                    Menu {
                        id: tableMenu
                        Material.roundedScale: theme.radius
                        MenuItem { text: qsTr("Show rows"); onTriggered: backend.preview(tableEntry.table.name) }
                        MenuItem {
                            text: tableEntry.open ? qsTr("Collapse") : qsTr("Expand")
                            onTriggered: panel.setOpen(tableEntry.table.name, !tableEntry.open)
                        }
                        MenuItem { text: qsTr("Insert name"); onTriggered: panel.insertRequested(backend.identifier(tableEntry.table.name)) }
                        MenuItem {
                            text: qsTr("Insert SELECT")
                            onTriggered: panel.insertRequested("SELECT " + tableEntry.table.columns.map(c => backend.identifier(c.name)).join(", ")
                                                               + "\nFROM " + backend.identifier(tableEntry.table.name) + "\nLIMIT 100;")
                        }
                        MenuItem { text: qsTr("Copy CREATE statement"); onTriggered: backend.copy(tableEntry.table.sql) }
                    }
                }

                Repeater {
                    model: tableEntry.open ? tableEntry.table.columns : []
                    delegate: Rectangle {
                        id: columnEntry
                        required property var modelData
                        readonly property bool hit: panel.query.length > 0 && modelData.name.toLowerCase().includes(panel.query)
                        width: tableEntry.width
                        height: modelData.references.length > 0 ? 40 : 26
                        color: columnHover.hovered ? theme.colors.lighterBackground : "transparent"

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 34
                            anchors.rightMargin: 12
                            spacing: 1

                            RowLayout {
                                width: parent.width
                                spacing: 6
                                Text {
                                    visible: columnEntry.modelData.primaryKey
                                    text: "PK"
                                    color: theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                                Text {
                                    visible: columnEntry.modelData.references.length > 0
                                    text: "FK"
                                    color: theme.colors.blue
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: columnEntry.modelData.name
                                    color: columnEntry.hit ? theme.accent : theme.colors.lightForeground
                                    font.pixelSize: 12
                                    font.family: win.mono
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: (columnEntry.modelData.type || "any").toLowerCase()
                                          + (columnEntry.modelData.notNull && !columnEntry.modelData.primaryKey ? " !" : "")
                                    color: theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    font.family: win.mono
                                }
                            }
                            Text {
                                visible: columnEntry.modelData.references.length > 0
                                text: "→ " + columnEntry.modelData.references
                                color: theme.colors.blue
                                font.pixelSize: 11
                                font.family: win.mono
                            }
                        }

                        HoverHandler { id: columnHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: panel.insertRequested(backend.identifier(columnEntry.modelData.name)) }
                        ToolTip.visible: columnHover.hovered
                        ToolTip.delay: 900
                        ToolTip.text: qsTr("Click to insert") + (columnEntry.modelData.defaultValue.length
                                      ? "\n" + qsTr("Default: %1").arg(columnEntry.modelData.defaultValue) : "")
                    }
                }

                Repeater {
                    model: tableEntry.open ? tableEntry.table.indexes : []
                    delegate: Text {
                        required property string modelData
                        x: 34
                        width: tableEntry.width - 46
                        height: 22
                        verticalAlignment: Text.AlignVCenter
                        text: "⌗ " + modelData
                        color: theme.colors.mutedForeground
                        font.pixelSize: 11
                        font.family: win.mono
                        elide: Text.ElideRight
                    }
                }
                Item { width: 1; height: tableEntry.open ? 6 : 0 }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: backend.tables.length === 0 ? qsTr("No tables yet") : qsTr("Nothing matches")
                color: theme.colors.mutedForeground
            }
        }
    }
}
