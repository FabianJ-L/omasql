import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Omasql

// The SQL editor: highlighting, line numbers, completion as you type, and
// running the statement under the cursor.
Rectangle {
    id: root
    color: "transparent"

    property bool loading: false
    property int previousLength: 0
    property int runBase: -1
    property int errorLine: -1
    // The statement that last failed when run from here, for the AI to fix:
    // where it was and what it said then.
    property int fixStart: -1
    property string fixSql: ""
    property string runText: ""
    readonly property bool canFix: fixSql.trim().length > 0
    property var completionItems: []
    property int completionStart: 0
    property bool completionVisible: false
    // Enter accepts a suggestion only once the user has chosen one (arrows or
    // Ctrl+Space); otherwise it is a new line, as typing "o⏎" should be.
    property bool completionChosen: false
    readonly property int fontSize: 14
    readonly property string indentUnit: "    "

    function load(text) {
        loading = true
        area.text = text
        area.cursorPosition = text.length
        previousLength = text.length
        highlighter.clearUndo()
        loading = false
        errorLine = -1
        fixSql = ""
        hideCompletion()
        if (backend.open)
            area.forceActiveFocus()
    }
    function saveNow() {
        if (backend.open)
            backend.saveDraft(area.text)
    }
    function focusEditor() { area.forceActiveFocus() }
    function insertText(text) {
        if (text.length > 0) {
            const at = area.selectionEnd > area.selectionStart ? area.selectionStart : area.cursorPosition
            const to = area.selectionEnd > area.selectionStart ? area.selectionEnd : at
            highlighter.replace(at, to, text)
            area.cursorPosition = at + text.length
        }
        area.forceActiveFocus()
    }
    function replaceAll(text) {
        highlighter.replace(0, area.text.length, text)
        area.cursorPosition = text.length
        area.forceActiveFocus()
    }

    // Puts the AI's version where the failed statement was, as one undoable
    // edit, and selects it for a look before it runs. False when the
    // statement is no longer in the editor.
    function applyFix(original, fixed) {
        let at = area.text.substring(fixStart, fixStart + original.length) === original ? fixStart : -1
        if (at < 0)
            at = area.text.indexOf(original)
        if (at < 0)
            return false
        highlighter.replace(at, at + original.length, fixed)
        area.select(at, at + fixed.length)
        errorLine = -1
        fixSql = ""
        hideCompletion()
        area.forceActiveFocus()
        return true
    }

    function runCurrent() {
        let sql, start, end
        if (area.selectionEnd > area.selectionStart) {
            start = area.selectionStart
            end = area.selectionEnd
            sql = area.text.substring(start, end)
        } else {
            const s = backend.statementAt(area.text, area.cursorPosition)
            start = s.start
            end = s.end
            sql = s.text
        }
        if (sql.trim().length === 0)
            return
        runBase = start
        runText = sql
        flash.show(start, end)
        win.requestRun(sql)
    }
    // The plan SQLite would use for the statement under the cursor (or the
    // selection), without running it.
    function explainCurrent() {
        const sql = area.selectionEnd > area.selectionStart
                  ? area.text.substring(area.selectionStart, area.selectionEnd)
                  : backend.statementAt(area.text, area.cursorPosition).text
        if (sql.trim().length === 0 || !backend.open || backend.running)
            return
        runBase = -1
        fixSql = ""
        backend.explain(sql)
    }
    function runAll() {
        if (area.text.trim().length === 0)
            return
        runBase = 0
        runText = area.text
        flash.show(0, area.text.length)
        win.requestRun(area.text)
    }

    // Completion ----------------------------------------------------------

    function requestCompletion(forced) {
        if (!backend.open)
            return
        const result = backend.completions(area.text, area.cursorPosition, forced)
        completionItems = result.items
        completionStart = result.start
        completionVisible = completionItems.length > 0
        completionChosen = forced
        list.currentIndex = 0
        list.positionViewAtBeginning()
    }
    function hideCompletion() {
        completionVisible = false
    }
    function acceptCompletion(index) {
        const item = completionItems[index]
        if (!item)
            return
        const at = completionStart
        highlighter.replace(at, area.cursorPosition, item.insert)
        area.cursorPosition = at + item.insert.length
        hideCompletion()
    }
    function completeAfterTyping() {
        const position = area.cursorPosition
        const typed = area.text.charAt(position - 1)
        if (/[\w$.]/.test(typed))
            requestCompletion(false)
        else
            hideCompletion()
    }
    function checkCompletion() {
        const word = area.text.substring(completionStart, area.cursorPosition)
        if (area.cursorPosition < completionStart || !/^[\w$]*$/.test(word))
            hideCompletion()
    }

    // Line editing ----------------------------------------------------------

    // Applies fn to every line the selection touches (or the cursor's line)
    // as one undoable replacement, keeping those lines selected.
    function transformLines(fn) {
        const text = area.text
        const from = text.lastIndexOf("\n", area.selectionStart - 1) + 1
        let to = text.indexOf("\n", Math.max(area.selectionEnd - (area.selectionEnd > area.selectionStart ? 1 : 0), from))
        if (to < 0)
            to = text.length
        const before = text.substring(from, to)
        const hadSelection = area.selectionEnd > area.selectionStart
        const cursorOffset = area.cursorPosition - from
        const after = fn(before.split("\n")).join("\n")
        if (after === before)
            return
        highlighter.replace(from, to, after)
        if (hadSelection)
            area.select(from, from + after.length)
        else
            area.cursorPosition = Math.max(from, Math.min(from + after.length, from + cursorOffset + (after.length - before.length)))
    }
    function toggleComment() {
        transformLines(lines => {
            const code = lines.filter(l => l.trim().length > 0)
            const commented = code.length > 0 && code.every(l => /^\s*--/.test(l))
            return lines.map(l => {
                if (l.trim().length === 0)
                    return l
                return commented ? l.replace(/^(\s*)-- ?/, "$1") : l.replace(/^(\s*)/, "$1-- ")
            })
        })
    }
    function indent() {
        if (area.selectionEnd > area.selectionStart && area.text.substring(area.selectionStart, area.selectionEnd).includes("\n"))
            transformLines(lines => lines.map(l => l.length ? root.indentUnit + l : l))
        else
            insertText(indentUnit)
    }
    function unindent() {
        transformLines(lines => lines.map(l => l.replace(/^ {1,4}|^\t/, "")))
    }
    function newline() {
        const text = area.text
        const hadSelection = area.selectionEnd > area.selectionStart
        const position = hadSelection ? area.selectionStart : area.cursorPosition
        const end = hadSelection ? area.selectionEnd : position
        const lineStart = text.lastIndexOf("\n", position - 1) + 1
        const indentation = text.substring(lineStart, position).match(/^[ \t]*/)[0]
        highlighter.replace(position, end, "\n" + indentation)
        area.cursorPosition = position + 1 + indentation.length
    }
    function lineOf(position) {
        return area.text.substring(0, position).split("\n").length - 1
    }

    Connections {
        target: backend
        // Only queries started from the editor point back into it.
        function onQueryFinished(ok, errorOffset) {
            const failed = !ok && root.runBase >= 0 && backend.lastError.length > 0
            root.fixStart = failed ? root.runBase : -1
            root.fixSql = failed ? root.runText : ""
            if (!ok && errorOffset >= 0 && root.runBase >= 0) {
                const position = Math.min(area.text.length, root.runBase + errorOffset)
                area.cursorPosition = position
                root.errorLine = root.lineOf(position)
                area.forceActiveFocus()
            }
            root.runBase = -1
        }
    }

    Timer {
        id: saveTimer
        interval: 800
        onTriggered: root.saveNow()
    }

    // Line numbers, laid out by a TextEdit like the editor so the lines match
    // exactly, and scrolled with it.
    Rectangle {
        id: gutter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: Math.max(44, numbers.implicitWidth + 20)
        color: Qt.alpha(theme.colors.darkBackground, 0.5)
        clip: true

        Item {
            id: gutterContent
            y: -flick.contentY
            width: gutter.width
            height: area.height

            Rectangle {
                visible: root.errorLine >= 0
                x: 0
                width: gutter.width
                y: root.errorLine >= 0 ? numbers.lineY(root.errorLine) : 0
                height: area.cursorRectangle.height
                color: Qt.alpha(theme.colors.red, 0.25)
            }

            Rectangle {
                id: flash
                property int fromY: 0
                property int toY: 0
                x: gutter.width - 3
                width: 3
                y: fromY
                height: Math.max(toY - fromY, 4)
                color: theme.accent
                opacity: 0
                function show(start, end) {
                    const a = area.positionToRectangle(start)
                    const b = area.positionToRectangle(Math.max(start, end))
                    fromY = a.y
                    toY = b.y + b.height
                    fade.restart()
                }
                SequentialAnimation {
                    id: fade
                    NumberAnimation { target: flash; property: "opacity"; to: 1; duration: 60 }
                    PauseAnimation { duration: 500 }
                    NumberAnimation { target: flash; property: "opacity"; to: 0.0; duration: 600 }
                }
            }

            TextEdit {
                id: numbers
                function lineY(line) {
                    let position = 0
                    for (let i = 0; i < line; ++i) {
                        const next = area.text.indexOf("\n", position)
                        if (next < 0) break
                        position = next + 1
                    }
                    return area.positionToRectangle(position).y
                }
                readOnly: true
                enabled: false
                anchors.right: parent.right
                anchors.rightMargin: 10
                topPadding: area.topPadding
                font.family: win.mono
                font.pixelSize: root.fontSize
                horizontalAlignment: TextEdit.AlignRight
                color: theme.colors.mutedForeground
                text: {
                    const count = area.lineCount
                    let lines = []
                    for (let i = 1; i <= count; ++i)
                        lines.push(i)
                    return lines.join("\n")
                }
            }
        }
    }

    Flickable {
        id: flick
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: gutter.right
        anchors.right: parent.right
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        TextArea.flickable: TextArea {
            id: area
            font.family: win.mono
            font.pixelSize: root.fontSize
            wrapMode: TextEdit.NoWrap
            textFormat: TextEdit.PlainText
            color: theme.colors.foreground
            selectionColor: theme.colors.selection
            selectedTextColor: theme.colors.brightForeground
            placeholderText: backend.open ? qsTr("Write SQL here. Ctrl+Enter runs the statement under the cursor, Ctrl+Space suggests names.") : ""
            placeholderTextColor: theme.colors.mutedForeground
            leftPadding: 12
            rightPadding: 12
            topPadding: 12
            bottomPadding: 12
            background: null
            persistentSelection: true
            selectByMouse: true
            Material.accent: theme.accent

            onTextChanged: {
                if (root.loading)
                    return
                const delta = text.length - root.previousLength
                root.previousLength = text.length
                root.errorLine = -1
                saveTimer.restart()
                if (!activeFocus)
                    return
                if (delta === 1 || (delta === -1 && root.completionVisible))
                    Qt.callLater(root.completeAfterTyping)
                else
                    root.hideCompletion()
            }
            onCursorPositionChanged: if (root.completionVisible) Qt.callLater(root.checkCompletion)
            onActiveFocusChanged: if (!activeFocus) root.hideCompletion()

            Keys.onShortcutOverride: event => {
                if (root.completionVisible && event.key === Qt.Key_Escape)
                    event.accepted = true
            }
            Keys.onPressed: event => {
                const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                const shift = (event.modifiers & Qt.ShiftModifier) !== 0
                if (root.completionVisible) {
                    switch (event.key) {
                    case Qt.Key_Down:
                        list.currentIndex = (list.currentIndex + 1) % list.count
                        root.completionChosen = true
                        event.accepted = true
                        return
                    case Qt.Key_Up:
                        list.currentIndex = (list.currentIndex - 1 + list.count) % list.count
                        root.completionChosen = true
                        event.accepted = true
                        return
                    case Qt.Key_PageDown:
                        list.currentIndex = Math.min(list.count - 1, list.currentIndex + 8)
                        event.accepted = true
                        return
                    case Qt.Key_PageUp:
                        list.currentIndex = Math.max(0, list.currentIndex - 8)
                        event.accepted = true
                        return
                    case Qt.Key_Escape:
                        root.hideCompletion()
                        event.accepted = true
                        return
                    case Qt.Key_Tab:
                        if (!ctrl && !shift) {
                            root.acceptCompletion(list.currentIndex)
                            event.accepted = true
                            return
                        }
                        break
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (!ctrl && root.completionChosen) {
                            root.acceptCompletion(list.currentIndex)
                            event.accepted = true
                            return
                        }
                        root.hideCompletion()
                        break
                    }
                }
                if (ctrl && event.key === Qt.Key_Space) {
                    root.requestCompletion(true)
                    event.accepted = true
                } else if ((ctrl && !shift && event.key === Qt.Key_Y) || (ctrl && shift && event.key === Qt.Key_Z)) {
                    // Both redo keys, whatever the platform's key map binds.
                    area.redo()
                    event.accepted = true
                } else if (ctrl && event.key === Qt.Key_Slash) {
                    root.toggleComment()
                    event.accepted = true
                } else if (event.key === Qt.Key_Backtab || (event.key === Qt.Key_Tab && shift)) {
                    root.unindent()
                    event.accepted = true
                } else if (event.key === Qt.Key_Tab && !ctrl) {
                    root.indent()
                    event.accepted = true
                } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !ctrl && !shift) {
                    root.newline()
                    event.accepted = true
                }
            }

            SqlHighlighter {
                id: highlighter
                textDocument: area.textDocument
                tables: backend.tableNames
                keywordColor: theme.colors.blue
                stringColor: theme.colors.green
                numberColor: theme.colors.orange
                commentColor: theme.colors.mutedForeground
                functionColor: theme.colors.cyan
                tableColor: theme.colors.yellow
                parameterColor: theme.colors.magenta
            }
        }
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        anchors.rightMargin: 18
        visible: area.activeFocus
        text: {
            const before = area.text.substring(0, area.cursorPosition)
            const line = before.split("\n").length
            const column = area.cursorPosition - before.lastIndexOf("\n")
            return qsTr("Ln %1, Col %2").arg(line).arg(column)
        }
        color: theme.colors.mutedForeground
        font.pixelSize: 11
    }

    // The completion list, under the cursor (above it near the bottom edge).
    Rectangle {
        id: popup
        // On the window, not in the editor pane, so it may reach over the
        // results when the cursor is near the pane's bottom edge.
        parent: Overlay.overlay
        visible: root.completionVisible && area.activeFocus
        z: 20
        width: Math.min(440, root.width - 16)
        height: Math.min(list.contentHeight, 9 * 28) + 24
        radius: Math.min(6, theme.radius)
        color: theme.colors.lighterBackground
        border.color: theme.colors.border

        readonly property rect cursor: area.cursorRectangle
        readonly property point below: {
            flick.contentY; flick.contentX
            root.x; root.y; root.width; root.height
            return area.mapToItem(parent, cursor.x, cursor.y + cursor.height)
        }
        x: Math.max(4, Math.min(below.x - 30, parent.width - width - 4))
        y: below.y + 4 + height > parent.height - 8 ? below.y - cursor.height - height - 4 : below.y + 4

        Text {
            id: popupHint
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 6
            anchors.rightMargin: 12
            text: qsTr("Tab to insert")
            color: theme.colors.mutedForeground
            font.pixelSize: 10
        }

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            anchors.bottomMargin: 20
            clip: true
            model: root.completionItems
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: suggestion
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 28
                radius: Math.min(4, theme.radius)
                color: ListView.isCurrentItem ? theme.colors.selection : "transparent"

                readonly property color kindColor: ({
                    "table": theme.colors.yellow, "view": theme.colors.magenta, "column": theme.accent,
                    "keyword": theme.colors.blue, "function": theme.colors.cyan
                })[modelData.kind] || theme.colors.foreground

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 8
                    spacing: 10
                    Rectangle {
                        width: 18
                        height: 18
                        radius: Math.min(3, theme.radius)
                        color: Qt.alpha(suggestion.kindColor, 0.18)
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: ({ "table": "T", "view": "V", "column": "C", "keyword": "K", "function": "ƒ" })[suggestion.modelData.kind]
                            color: suggestion.kindColor
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                    Text {
                        text: suggestion.modelData.label
                        color: theme.colors.brightForeground
                        font.family: win.mono
                        font.pixelSize: 13
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.width * 0.45)
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    text: suggestion.modelData.detail
                    color: theme.colors.mutedForeground
                    font.pixelSize: 11
                }
                TapHandler { onTapped: root.acceptCompletion(suggestion.index) }
            }
        }
    }
}
