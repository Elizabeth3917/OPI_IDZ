#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QFileInfo>
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include <memory>

// ---------------- Interfaces for Abstract Factory ----------------
class IFileLoader {
public:
    virtual ~IFileLoader() = default;
    virtual QString load(const QString &path) = 0;
};

class IFileSaver {
public:
    virtual ~IFileSaver() = default;
    virtual bool save(const QString &path, const QString &text) = 0;
};

class IFileFactory {
public:
    virtual ~IFileFactory() = default;
    virtual std::unique_ptr<IFileLoader> createLoader() = 0;
    virtual std::unique_ptr<IFileSaver> createSaver() = 0;
};

// ---------------- TXT ----------------
class TXTLoader : public IFileLoader {
public:
    QString load(const QString &path) override {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QTextStream in(&f);
        return in.readAll();
    }
};
class TXTSaver : public IFileSaver {
public:
    bool save(const QString &path, const QString &text) override {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&f);
        out << text;
        return true;
    }
};
class TXTFactory : public IFileFactory {
public:
    std::unique_ptr<IFileLoader> createLoader() override { return std::make_unique<TXTLoader>(); }
    std::unique_ptr<IFileSaver>  createSaver() override { return std::make_unique<TXTSaver>(); }
};

// ---------------- HTML ----------------
static QString htmlToPlain(const QString &html) {
    QString s = html;
    QString out;
    bool inTag = false;
    QString tag;
    for (int i = 0; i < s.size(); ++i) {
        QChar c = s[i];
        if (c == '<') { inTag = true; tag.clear(); continue; }
        if (inTag) {
            if (c == '>') {
                inTag = false;
                QString t = tag.trimmed().toLower();
                if (t.startsWith("br") || t.startsWith("br/")) out += "\n\n";
                if (t.startsWith("p") || t.startsWith("/p")) out += "\n\n";
            } else {
                tag += c;
            }
            continue;
        }
        // normal char
        if (!inTag) out += c;
    }
    QStringList lines = out.split('\n');
    QString result;
    int emptyCount = 0;
    for (QString ln : lines) {
        if (ln.trimmed().isEmpty()) { emptyCount++; if (emptyCount <= 2) result += "\n"; }
        else { emptyCount = 0; result += ln + "\n"; }
    }
    return result.trimmed();
}

class HTMLLoader : public IFileLoader {
public:
    QString load(const QString &path) override {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QByteArray bytes = f.readAll();
        QString html = QString::fromUtf8(bytes);
        return htmlToPlain(html);
    }
};
class HTMLSaver : public IFileSaver {
public:
    bool save(const QString &path, const QString &text) override {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream out(&f);
        out << "<html><body>\n";
        QStringList paras = text.split("\n\n", Qt::SkipEmptyParts);
        for (const QString &p : paras) {
            out << "<p>" << p.toHtmlEscaped() << "</p>\n";
        }
        out << "\n</body></html>\n";
        return true;
    }
};
class HTMLFactory : public IFileFactory {
public:
    std::unique_ptr<IFileLoader> createLoader() override { return std::make_unique<HTMLLoader>(); }
    std::unique_ptr<IFileSaver>  createSaver() override { return std::make_unique<HTMLSaver>(); }
};

// ---------------- BIN ----------------
class BINLoader : public IFileLoader {
public:
    QString load(const QString &path) override {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QByteArray bytes = f.readAll();
        return QString::fromUtf8(bytes);
    }
};

class BINSaver : public IFileSaver {
public:
    bool save(const QString &path, const QString &text) override {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        QByteArray bytes = text.toUtf8();
        f.write(bytes);
        return true;
    }
};

class BINFactory : public IFileFactory {
public:
    std::unique_ptr<IFileLoader> createLoader() override { return std::make_unique<BINLoader>(); }
    std::unique_ptr<IFileSaver>  createSaver() override { return std::make_unique<BINSaver>(); }
};

// ---------------- Observer (Subject + Observer) ----------------
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onParagraphsDeleted(int count) = 0;
    virtual void onAutoSaved(const QString &path) = 0;
};

class Subject {
    QList<IObserver*> obs;
public:
    void add(IObserver* o) { if (o && !obs.contains(o)) obs.append(o); }
    void remove(IObserver* o) { obs.removeAll(o); }
    void notifyDeleted(int n) { for (auto o : obs) o->onParagraphsDeleted(n); }
    void notifySaved(const QString &p) { for (auto o : obs) o->onAutoSaved(p); }
};

class MessageObserver : public IObserver {
    QWidget *parent = nullptr;
public:
    MessageObserver(QWidget *p = nullptr): parent(p) {}
    void onParagraphsDeleted(int count) override {
        QMessageBox::information(parent, "Абзаци видалено", QString("Видалено абзаців: %1").arg(count));
    }
    void onAutoSaved(const QString &path) override {
        QMessageBox::information(parent, "Автозбереження", QString("Файл оновлено: %1").arg(path));
    }
};

// ---------------- Utility: paragraph counting ----------------
static int countParagraphs(const QString &text) {
    QStringList paras;
    QStringList lines = text.split('\n');
    QString cur;
    for (QString ln : lines) {
        if (ln.trimmed().isEmpty()) {
            if (!cur.isEmpty()) { paras << cur; cur.clear(); }
        } else {
            if (!cur.isEmpty()) cur += "\n";
            cur += ln;
        }
    }
    if (!cur.isEmpty()) paras << cur;
    return paras.count();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Простий текстовий редактор (AbstractFactory + Observer)");
    QVBoxLayout *layout = new QVBoxLayout(&window);

    QMenuBar *menuBar = new QMenuBar();
    QMenu *menuFile = menuBar->addMenu("File");
    QAction *actOpen = menuFile->addAction("Відкрити...");
    QAction *actSave = menuFile->addAction("Зберегти...");
    menuFile->addSeparator();
    QAction *actExit = menuFile->addAction("Вихід");

    QTextEdit *txt = new QTextEdit();
    txt->setAcceptRichText(false);
    layout->setMenuBar(menuBar);
    layout->addWidget(txt);

    // State
    QString currentPath;
    std::unique_ptr<IFileFactory> currentFactory; // factory for current file extension

    // Subject / observer
    Subject subject;
    MessageObserver msgObs(&window);
    subject.add(&msgObs);

    int lastParagraphCount = 0;
    lastParagraphCount = countParagraphs(txt->toPlainText());

    auto factoryForExtension = [](const QString &ext)->std::unique_ptr<IFileFactory> {
        if (ext == "txt") return std::make_unique<TXTFactory>();
        if (ext == "html" || ext == "htm") return std::make_unique<HTMLFactory>();
        if (ext == "bin") return std::make_unique<BINFactory>();
        return std::make_unique<TXTFactory>();
    };

    QObject::connect(actOpen, &QAction::triggered, [&]() {
        QString fname = QFileDialog::getOpenFileName(&window, "Відкрити файл", "", "All Files (*.*)");
        if (fname.isEmpty()) return;
        QFileInfo fi(fname);
        QString ext = fi.suffix().toLower();
        currentFactory = factoryForExtension(ext);
        auto loader = currentFactory->createLoader();
        QString content = loader->load(fname);
        txt->setPlainText(content);
        currentPath = fname;
        lastParagraphCount = countParagraphs(content);
    });

    QObject::connect(actSave, &QAction::triggered, [&]() {
        if (currentPath.isEmpty()) {
            QString fname = QFileDialog::getSaveFileName(&window, "Зберегти файл", "", "All Files (*.*)");
            if (fname.isEmpty()) return;
            currentPath = fname;
            QFileInfo fi(fname);
            currentFactory = factoryForExtension(fi.suffix().toLower());
        }
        if (!currentFactory) currentFactory = factoryForExtension(QFileInfo(currentPath).suffix().toLower());
        auto saver = currentFactory->createSaver();
        bool ok = saver->save(currentPath, txt->toPlainText());
        if (!ok) QMessageBox::warning(&window, "Помилка", "Не вдалося зберегти файл.");
        else subject.notifySaved(currentPath);
    });

    QObject::connect(actExit, &QAction::triggered, &app, &QApplication::quit);

    QObject::connect(txt, &QTextEdit::textChanged, [&]() {
        QString text = txt->toPlainText();
        int curCount = countParagraphs(text);
        if (curCount < lastParagraphCount) {
            int deleted = lastParagraphCount - curCount;
            subject.notifyDeleted(deleted);
        } else if (curCount > lastParagraphCount) {
            if (!currentPath.isEmpty()) {
                if (!currentFactory) currentFactory = factoryForExtension(QFileInfo(currentPath).suffix().toLower());
                auto saver = currentFactory->createSaver();
                saver->save(currentPath, text);
                subject.notifySaved(currentPath);
            }
        }
        lastParagraphCount = curCount;
    });

    window.resize(800, 600);
    window.show();
    return app.exec();
}
