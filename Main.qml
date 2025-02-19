// Main.qml
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import CameraCore 1.0

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 800
    height: 600
    title: "Camera App"

    // 视频显示区域
    Image {
        id: videoDisplay
        anchors.fill: parent
        source: "image://videoprovider/frame"
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false

        // 硬件加速配置
        layer.enabled: true
        layer.textureMirroring: Qt.verticallyMirrored
    }

    // 控制面板
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 120
        color: "#33000000"

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            // 设备选择行
            RowLayout {
                ComboBox {
                    id: deviceCombo
                    model: cameraController.devices
                    Layout.preferredWidth: 200
                    delegate: ItemDelegate {
                        width: deviceCombo.width
                        text: modelData
                    }
                }
                // 设备初始化时刷新
                // Button {
                //     text: "刷新设备"
                //     onClicked: cameraController.refreshDevices()
                // }
            }

            // 摄像头配置行
            RowLayout {
                ComboBox {
                    id: formatCombo
                    model: ["YUYV", "MJPG", "NV12"]
                    Layout.preferredWidth: 150
                }

                ComboBox {
                    id: resolutionCombo
                    model: ["640x480", "1280x720", "1920x1080"]
                    Layout.preferredWidth: 150
                }

                Button {
                    text: "开始"
                    onClicked: cameraController.startCapture(
                        deviceCombo.currentText,
                        formatCombo.currentText,
                        resolutionCombo.currentText
                    )
                }
            }

            // 底层按钮行
            RowLayout {
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter

                RoundButton {
                    icon.source: "qrc:/icon/takepic_1.svg"
                    icon.width: 48
                    icon.height: 48
                    onClicked: cameraController.takePhoto()
                }

                RoundButton {
                    icon.source: "qrc:/icon/gallery.svg"
                    icon.width: 48
                    icon.height: 48
                    onClicked: galleryPopup.open()
                }
            }
        }
    }

    // 相册弹窗
    Popup {
        id: galleryPopup
        width: parent.width * 0.8
        height: parent.height * 0.8
        modal: true

        GridView {
            anchors.fill: parent
            cellWidth: 150
            cellHeight: 150
            model: galleryModel
            delegate: Image {
                width: 140
                height: 140
                source: "file://" + model.path
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    // 状态提示
    Label {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: cameraController.statusMessage
        color: "white"
        font.bold: true
        visible: text !== ""
    }
}