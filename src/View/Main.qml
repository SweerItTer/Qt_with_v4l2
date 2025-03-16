// Main.qml
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml 2.12
// import CameraCore 1.0

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
        //"qrc:pic/View/pic/test.png"
        source: "image://videoprovider/frame" // 视频帧
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false

        // 硬件加速配置
        layer.enabled: true
        layer.smooth: true
    }

    // 控制面板
    Rectangle {
        // 覆盖在视频显示区域上方
        anchors.top: parent.top // 固定在顶部
        anchors.left : parent.left // 固定在左边
        opacity: 0.5 // 半透明
        width: parent.width
        height: parent.height * 0.1 // 控制面板高度
        color: "#33000000"

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            // 摄像头配置行
            RowLayout {
                // 摄像头选择
                MyComboBox {
                    id: deviceCombo
                    model: ["/dev/video31"]//cameraController.devices
                    Layout.preferredWidth: 200
                }

                // 格式选择
                MyComboBox {
                    id: formatCombo
                    model: ["YUYV", "MJPG", "NV12"]
                    Layout.preferredWidth: 150
                }
                // 分辨率选择
                MyComboBox {
                    id: resolutionCombo
                    model: ["640x480", "1280x720", "1920x1080"]
                    Layout.preferredWidth: 150
                }
                // 打开摄像头按钮
                Button {
                    text: "Open Camera"
                    
                    background: Rectangle {
                        color: "#333333"
                        border.color: "#666666"
                        radius: 4
                        
                        MouseArea {
                            anchors.fill: parent
                            onPressed: parent.color = "#555555"
                            onReleased: parent.color = "#666666"

                            onClicked: console.log("open camera ")
                        }
                        
                        // onClicked: cameraController.startCapture(
                        //     deviceCombo.currentText,
                        //     formatCombo.currentText,
                        //     resolutionCombo.currentText       
                        // )
                    }
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

        // GridView {
        //     anchors.fill: parent
        //     cellWidth: 150
        //     cellHeight: 150
        //     model: ["pic/test.png"]//galleryModel
        //     delegate: Image {
        //         width: 140
        //         height: 140
        //         source: "file://" + modelData
        //         fillMode: Image.PreserveAspectFit
        //     }
        // }
    }

    // 底层按钮行
    RowLayout {
        id: bottomButtons
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        
        width: parent.width
        
        spacing: 20

        Item {// 弹簧
            height: 1
            Layout.fillWidth: true
        }
        RoundButton {
            property bool isPressed: false

            icon.source: isPressed ? "qrc:/icon/View/icon/takepic_2.svg" : "qrc:/icon/View/icon/takepic_1.svg"
            icon.width: 48
            icon.height: 48

            onPressed: isPressed = true
            onReleased: isPressed = false
            onClicked: console.log("takePhoto")   //cameraController.takePhoto()
        }

        Item{// 弹簧
            height: 1
            Layout.fillWidth: true
        }
    }
    Button {
        id: galleryButton
        anchors.left:bottomButtons.left
        anchors.bottom: bottomButtons.bottom
        anchors.bottomMargin: 10
        text: "Gallery"
        // icon.source: "qrc:View/icon/gallery.svg"
        icon.width: 48
        icon.height: 48
        onClicked: galleryPopup.open()
    }

    // // 状态提示
    // Label {
    //     anchors.top: parent.top
    //     anchors.horizontalCenter: parent.horizontalCenter
    //     text: cameraController.statusMessage
    //     color: "white"
    //     font.bold: true
    //     visible: text !== ""
    // }
}