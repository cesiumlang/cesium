#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Create main window
    QWidget window;
    window.setWindowTitle("Cesium Qt Hello World");
    window.resize(300, 200);

    // Create layout and widgets
    QVBoxLayout *layout = new QVBoxLayout;
    
    QLabel *label = new QLabel("Hello from Cesium with Qt!");
    label->setAlignment(Qt::AlignCenter);
    
    QPushButton *button = new QPushButton("Click me!");
    
    // Connect button click to show message
    QObject::connect(button, &QPushButton::clicked, [&]() {
        QMessageBox::information(&window, "Hello", "Button clicked in Cesium Qt app!");
    });
    
    // Add widgets to layout
    layout->addWidget(label);
    layout->addWidget(button);
    
    // Set layout to window
    window.setLayout(layout);
    
    // Show window
    window.show();

    return app.exec();
}
