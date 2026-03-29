#include <QApplication>
#include <QTranslator>

#include "src/app/app_settings.h"
#include "src/app/app_context.h"
#include "src/ui/lucide_icon_factory.h"
#include "src/ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("pycdc-studio"));
    QApplication::setOrganizationName(QStringLiteral("pycdc-studio"));
    app.setWindowIcon(LucideIconFactory::appIcon());

    QTranslator translator;
    AppSettings::installTranslator(app, translator);
    AppSettings::applyStyleSheet(app);

    AppContext context;
    MainWindow window(&context);
    window.show();

    return app.exec();
}
