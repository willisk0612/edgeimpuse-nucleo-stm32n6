import sys
import time
from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QPushButton,
    QLabel,
    QVBoxLayout,
    QHBoxLayout,
    QMessageBox,
    QTextEdit,
    QComboBox,
    QSpinBox,
    QFormLayout,
    QGroupBox,
)
from PySide6.QtGui import QPainter, QPen, QPixmap, QImage, QTextCursor
from PySide6.QtCore import Qt, QThread, Signal, QPoint
from PIL import Image
import numpy as np
import cv2
from serial import Serial
from serial.tools import list_ports

IMG_SIZE = 32
CANVAS_SIZE = 280
INK_THICKNESS = 80
PADDING = 25
DEFAULT_BAUDRATE = 9600
DEFAULT_COM = "COM3"
DEFAULT_STOPBITS = 1
DEFAULT_PARITY = "N"
DEFAULT_FLOWCONTROL = False
DEFAULT_DATABITS = 8

LAPLACIAN_BLUR_THRESHOLD = 3000
GAUSSIAN_BLUR_SIGMA = 0.8
SHARPEN_WEIGHT = 1.2
SHARPEN_BLUR_WEIGHT = -0.2
BRIGHTNESS_MULTIPLIER = 1.1
SERIAL_POLL_INTERVAL = 0.05


class Canvas(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.pixmap = QPixmap(CANVAS_SIZE, CANVAS_SIZE)
        self.pixmap.fill(Qt.black)
        self.last_point = None

    def resizeEvent(self, event):
        size = min(self.width(), self.height())
        old_pixmap = self.pixmap
        self.pixmap = QPixmap(size, size)
        self.pixmap.fill(Qt.black)

        if not old_pixmap.isNull():
            painter = QPainter(self.pixmap)
            painter.drawPixmap(0, 0, old_pixmap)

        super().resizeEvent(event)

    def paintEvent(self, event):
        painter = QPainter(self)
        x = (self.width() - self.pixmap.width()) // 2
        y = (self.height() - self.pixmap.height()) // 2
        painter.drawPixmap(x, y, self.pixmap)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            pixmap_point = self.widget_to_pixmap(event.position().toPoint())
            if pixmap_point:
                self.last_point = pixmap_point

    def mouseMoveEvent(self, event):
        if event.buttons() & Qt.LeftButton and self.last_point is not None:
            pixmap_point = self.widget_to_pixmap(event.position().toPoint())
            if pixmap_point:
                painter = QPainter(self.pixmap)
                pen = QPen(
                    Qt.white, INK_THICKNESS, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin
                )
                painter.setPen(pen)
                painter.drawLine(self.last_point, pixmap_point)
                self.last_point = pixmap_point
                self.update()

    def widget_to_pixmap(self, widget_point):
        x_offset = (self.width() - self.pixmap.width()) // 2
        y_offset = (self.height() - self.pixmap.height()) // 2
        pixmap_x = widget_point.x() - x_offset
        pixmap_y = widget_point.y() - y_offset

        return (
            QPoint(pixmap_x, pixmap_y)
            if (
                0 <= pixmap_x < self.pixmap.width()
                and 0 <= pixmap_y < self.pixmap.height()
            )
            else None
        )

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.last_point = None

    def clear(self):
        self.pixmap.fill(Qt.black)
        self.update()

    def get_image(self):
        qimage: QImage = self.pixmap.toImage().convertToFormat(QImage.Format_ARGB32)
        width, height = qimage.width(), qimage.height()
        ptr = qimage.bits()
        ptr = ptr[: qimage.sizeInBytes()]
        img = Image.frombuffer("RGBA", (width, height), bytes(ptr), "raw", "BGRA", 0, 1)
        return img.convert("L")


class SerialReaderThread(QThread):
    message_received = Signal(str)

    def __init__(self, serial_port):
        super().__init__()
        self.serial_port = serial_port
        self.running = True

    def run(self):
        while self.running and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    message = (
                        self.serial_port.readline()
                        .decode("utf-8", errors="ignore")
                        .strip()
                    )
                    if message:
                        self.message_received.emit(message)
                time.sleep(SERIAL_POLL_INTERVAL)
            except Exception as e:
                self.message_received.emit(f"Error reading from serial: {str(e)}")
                break

    def stop(self):
        self.running = False


class HandwritingLiveApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Handwriting Live - MNIST Classifier")
        self.serial_port = None
        self.serial_thread = None

        self.setup_ui()
        self.populate_com_ports()

    def process_canvas_image(self, img):
        img_array = np.array(img)

        ink = np.where(img_array > 0)
        if ink[0].size > 0 and ink[1].size > 0:
            min_y, max_y = ink[0].min(), ink[0].max()
            min_x, max_x = ink[1].min(), ink[1].max()
            h, w = max_y - min_y + 1, max_x - min_x + 1
            pad = int((PADDING / 100.0) * h)
            min_y, max_y = max(min_y - pad, 0), min(max_y + pad, img_array.shape[0] - 1)
            min_x, max_x = max(min_x - pad, 0), min(max_x + pad, img_array.shape[1] - 1)
            cropped = img_array[min_y : max_y + 1, min_x : max_x + 1]
        else:
            cropped = img_array

        h, w = cropped.shape
        scale = IMG_SIZE / max(h, w)
        new_h, new_w = int(h * scale), int(w * scale)
        resized = cv2.resize(cropped, (new_w, new_h), interpolation=cv2.INTER_NEAREST)

        final_img = np.zeros((IMG_SIZE, IMG_SIZE), dtype=np.uint8)
        y_off = (IMG_SIZE - new_h) // 2
        x_off = (IMG_SIZE - new_w) // 2
        final_img[y_off : y_off + new_h, x_off : x_off + new_w] = resized

        norm = cv2.normalize(
            final_img, None, alpha=0, beta=255, norm_type=cv2.NORM_MINMAX
        )

        lap_var = cv2.Laplacian(norm, cv2.CV_64F).var()
        if lap_var < LAPLACIAN_BLUR_THRESHOLD:
            blur = cv2.GaussianBlur(norm, (0, 0), GAUSSIAN_BLUR_SIGMA)
            norm = cv2.addWeighted(norm, SHARPEN_WEIGHT, blur, SHARPEN_BLUR_WEIGHT, 0)

        norm = np.clip(norm * BRIGHTNESS_MULTIPLIER, 0, 255).astype(np.uint8)

        return Image.fromarray(norm)

    def setup_ui(self):
        # Canvas for drawing
        self.canvas = Canvas()

        # COM port configuration group
        com_group = QGroupBox("COM Port Settings")
        com_layout = QFormLayout()

        self.port_combo = QComboBox()
        self.baudrate_spin = QSpinBox()
        self.baudrate_spin.setRange(1200, 921600)
        self.baudrate_spin.setValue(DEFAULT_BAUDRATE)

        com_layout.addRow("Port:", self.port_combo)
        com_layout.addRow("Baud Rate:", self.baudrate_spin)
        com_group.setLayout(com_layout)

        # Connection buttons
        connection_layout = QHBoxLayout()
        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)
        self.refresh_button = QPushButton("Refresh Ports")
        self.refresh_button.clicked.connect(self.populate_com_ports)

        connection_layout.addWidget(self.connect_button)
        connection_layout.addWidget(self.refresh_button)

        # Drawing control buttons
        drawing_layout = QHBoxLayout()
        self.send_button = QPushButton("Send Image")
        self.send_button.clicked.connect(self.send_image)
        self.send_button.setEnabled(False)

        self.clear_button = QPushButton("Clear")
        self.clear_button.clicked.connect(self.canvas.clear)

        drawing_layout.addWidget(self.send_button)
        drawing_layout.addWidget(self.clear_button)

        # Messages display
        messages_group = QGroupBox("Received Messages")
        messages_layout = QVBoxLayout()

        self.messages_text = QTextEdit()
        self.messages_text.setReadOnly(True)
        self.messages_text.setMaximumHeight(100)

        self.clear_messages_button = QPushButton("Clear Messages")
        self.clear_messages_button.clicked.connect(self.clear_messages)

        messages_layout.addWidget(self.messages_text)
        messages_layout.addWidget(self.clear_messages_button)
        messages_group.setLayout(messages_layout)

        main_layout = QVBoxLayout()
        main_layout.addWidget(com_group)
        main_layout.addLayout(connection_layout)
        main_layout.addWidget(QLabel("Draw your digit:"))
        main_layout.addWidget(self.canvas, 1)
        main_layout.addLayout(drawing_layout)
        main_layout.addWidget(messages_group)

        self.setLayout(main_layout)

    def populate_com_ports(self):
        self.port_combo.clear()
        ports = list_ports.comports()

        if ports:
            default_index = -1
            for i, port in enumerate(ports):
                port_text = f"{port.device} - {port.description}"
                self.port_combo.addItem(port_text)
                if port.device == DEFAULT_COM:
                    default_index = i

            if default_index != -1:
                self.port_combo.setCurrentIndex(default_index)
        else:
            self.port_combo.addItem("No COM ports found")

    def toggle_connection(self):
        if self.serial_port is None or not self.serial_port.is_open:
            self.connect_to_port()
        else:
            self.disconnect_from_port()

    def connect_to_port(self):
        if self.port_combo.currentText() == "No COM ports found":
            QMessageBox.warning(self, "Warning", "No COM ports available!")
            return

        port_text = self.port_combo.currentText()
        port_name = port_text.split(" - ")[0]
        baudrate = self.baudrate_spin.value()

        try:
            self.serial_port = Serial(
                port=port_name,
                baudrate=baudrate,
                bytesize=DEFAULT_DATABITS,
                parity=DEFAULT_PARITY,
                stopbits=DEFAULT_STOPBITS,
                timeout=1,
                xonxoff=DEFAULT_FLOWCONTROL,
                rtscts=DEFAULT_FLOWCONTROL,
                dsrdtr=DEFAULT_FLOWCONTROL,
            )

            self.serial_thread = SerialReaderThread(self.serial_port)
            self.serial_thread.message_received.connect(self.display_message)
            self.serial_thread.start()

            self.connect_button.setText("Disconnect")
            self.send_button.setEnabled(True)
            self.port_combo.setEnabled(False)
            self.baudrate_spin.setEnabled(False)

            self.display_message(f"Connected to {port_name} at {baudrate} baud")

        except Exception as e:
            QMessageBox.critical(
                self, "Connection Error", f"Failed to connect to {port_name}:\n{str(e)}"
            )

    def disconnect_from_port(self):
        if self.serial_thread:
            self.serial_thread.stop()
            self.serial_thread.wait()
            self.serial_thread = None

        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.serial_port = None

        self.connect_button.setText("Connect")
        self.send_button.setEnabled(False)
        self.port_combo.setEnabled(True)
        self.baudrate_spin.setEnabled(True)

        self.display_message("Disconnected from COM port")

    def send_image(self):
        if not self.serial_port or not self.serial_port.is_open:
            QMessageBox.warning(self, "Warning", "Not connected to a COM port!")
            return

        img = self.canvas.get_image()

        if img.getextrema() == (0, 0):
            QMessageBox.warning(self, "Empty Image", "Please draw a digit first!")
            return

        img = self.process_canvas_image(img)

        try:
            img_array = np.array(img)
            if img_array.shape != (IMG_SIZE, IMG_SIZE):
                raise ValueError(f"Image size mismatch: {img_array.shape}")

            img_array_int8 = (img_array - 128).astype(np.int8)
            img_bytes = img_array_int8.tobytes()

            self.serial_port.write(img_bytes)
            self.serial_port.flush()

        except Exception as e:
            QMessageBox.critical(self, "Send Error", f"Failed to send image:\n{str(e)}")

    def display_message(self, message):
        skip_messages = [
            "Data received",
            "Data stats:",
            "Running classifier",
            "Prediction: Digit",
            "Received 1024 bytes",
        ]

        if any(skip in message for skip in skip_messages):
            return

        timestamp = time.strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        self.messages_text.append(formatted_message)

        cursor = self.messages_text.textCursor()
        cursor.movePosition(QTextCursor.End)
        self.messages_text.setTextCursor(cursor)

    def clear_messages(self):
        self.messages_text.clear()

    def closeEvent(self, event):
        self.disconnect_from_port()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = HandwritingLiveApp()
    window.show()
    sys.exit(app.exec())
