#pragma once

#include <QObject>
#include <QString>

// Asks the user for a file. The real one goes through xdg-desktop-portal;
// tests hand the backend a fake. The pattern follows Omacut's filepicker.h
// (MIT, David Heinemeier Hansson).
class FilePicker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~FilePicker() override = default;

    virtual void openDatabase(const QString &title) = 0;
    // filterName and pattern describe the one file type offered, like
    // "CSV files" and "*.csv".
    virtual void saveFile(const QString &title, const QString &suggestedName,
                          const QString &filterName, const QString &pattern) = 0;

signals:
    void picked(const QString &path);
    void cancelled();
    void failed(const QString &message);
};
