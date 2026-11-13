#pragma once

#include "filepicker.h"

#include <QVariantMap>

// Open and save dialogs through org.freedesktop.portal.FileChooser, so the
// desktop's own chooser shows up instead of Qt's.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openDatabase(const QString &title) override;
    void saveCsv(const QString &title, const QString &suggestedName) override;

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    void request(const QString &method, const QString &title, QVariantMap options);
    void listen(const QString &path);
    void stopListening();

    QString m_requestPath;
    int m_counter = 0;
};
