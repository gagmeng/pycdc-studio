#include <QApplication>

#include "src/app/app_context.h"
#include "src/ui/lucide_icon_factory.h"
#include "src/ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("pycdc-studio"));
    QApplication::setOrganizationName(QStringLiteral("pycdc-studio"));
    app.setWindowIcon(LucideIconFactory::appIcon());

    AppContext context;
    MainWindow window(&context);
    window.show();

    return app.exec();
}
