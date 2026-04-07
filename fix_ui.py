import xml.etree.ElementTree as ET

tree = ET.parse('mainwindow.ui')
root = tree.getroot()

# Find centralwidget
for widget in root.iter('widget'):
    if widget.get('name') == 'centralwidget':
        # check if it has a styleSheet property
        has_ss = False
        for prop in widget.findall('property'):
            if prop.get('name') == 'styleSheet':
                prop.find('string').text = """
QGroupBox#groupBox_2, QGroupBox#groupBox, QGroupBox#groupBox_4 {
  border-radius: 10px;
  margin-top: 12px;
}
QGroupBox#groupBox_2 {
  border: 1px solid rgba(0, 200, 255, 0.45);
  background: rgba(8, 24, 42, 0.62);
}
QGroupBox#groupBox {
  border: 1px solid rgba(0, 170, 255, 0.38);
  background: rgba(10, 28, 48, 0.58);
}
QGroupBox#groupBox_4 {
  border: 1px solid rgba(255, 120, 120, 0.55);
  background: rgba(56, 16, 16, 0.62);
}

QGroupBox#groupBox_2::title, QGroupBox#groupBox::title, QGroupBox#groupBox_4::title {
  subcontrol-origin: margin;
  subcontrol-position: top center;
  padding: 0 10px;
  font-weight: bold;
}
QGroupBox#groupBox_2::title { color: #7fdfff; background: rgba(8, 24, 42, 0.9); border-bottom: 1px solid rgba(0, 200, 255, 0.45); border-radius: 4px; }
QGroupBox#groupBox::title { color: #9fd9ff; background: rgba(10, 28, 48, 0.9); border-bottom: 1px solid rgba(0, 170, 255, 0.38); border-radius: 4px; }
QGroupBox#groupBox_4::title { color: #ff9999; background: rgba(56, 16, 16, 0.9); border-bottom: 1px solid rgba(255, 120, 120, 0.55); border-radius: 4px; }

#groupBox_2 QToolButton, #groupBox QToolButton {
  color: #dff6ff;
  background: rgba(16, 52, 85, 0.70);
  border: 1px solid rgba(0, 200, 255, 0.40);
  border-radius: 8px;
  padding: 3px 8px;
  min-height: 36px;
}
#groupBox_2 QToolButton:hover, #groupBox QToolButton:hover {
  background: rgba(24, 80, 125, 0.86);
  border: 1px solid #00c8ff;
}

#TBtn_RemoveWarning {
  color: #fff4f4;
  background: rgba(128, 24, 24, 0.92);
  border: 1px solid #ff9a9a;
  border-radius: 8px;
  padding: 3px 8px;
  min-height: 36px;
}
#TBtn_RemoveWarning:hover {
  background: rgba(176, 35, 35, 0.96);
  border: 1px solid #ffd6d6;
}

#groupBox QToolButton:checked {
  background: rgba(0, 130, 200, 0.95);
  border: 1px solid #9fe7ff;
  color: #ffffff;
}
"""
                has_ss = True
        
        if not has_ss:
            prop = ET.SubElement(widget, 'property')
            prop.set('name', 'styleSheet')
            string = ET.SubElement(prop, 'string')
            string.set('notr', 'true')
            string.text = """
QGroupBox#groupBox_2, QGroupBox#groupBox, QGroupBox#groupBox_4 {
  border-radius: 10px;
  margin-top: 12px;
}
QGroupBox#groupBox_2 {
  border: 1px solid rgba(0, 200, 255, 0.45);
  background: rgba(8, 24, 42, 0.62);
}
QGroupBox#groupBox {
  border: 1px solid rgba(0, 170, 255, 0.38);
  background: rgba(10, 28, 48, 0.58);
}
QGroupBox#groupBox_4 {
  border: 1px solid rgba(255, 120, 120, 0.55);
  background: rgba(56, 16, 16, 0.62);
}

QGroupBox#groupBox_2::title, QGroupBox#groupBox::title, QGroupBox#groupBox_4::title {
  subcontrol-origin: margin;
  subcontrol-position: top center;
  padding: 0 10px;
  font-weight: bold;
}
QGroupBox#groupBox_2::title { color: #7fdfff; background: rgba(8, 24, 42, 0.9); border-bottom: 1px solid rgba(0, 200, 255, 0.45); border-radius: 4px; }
QGroupBox#groupBox::title { color: #9fd9ff; background: rgba(10, 28, 48, 0.9); border-bottom: 1px solid rgba(0, 170, 255, 0.38); border-radius: 4px; }
QGroupBox#groupBox_4::title { color: #ff9999; background: rgba(56, 16, 16, 0.9); border-bottom: 1px solid rgba(255, 120, 120, 0.55); border-radius: 4px; }

#groupBox_2 QToolButton, #groupBox QToolButton {
  color: #dff6ff;
  background: rgba(16, 52, 85, 0.70);
  border: 1px solid rgba(0, 200, 255, 0.40);
  border-radius: 8px;
  padding: 3px 8px;
  min-height: 36px;
}
#groupBox_2 QToolButton:hover, #groupBox QToolButton:hover {
  background: rgba(24, 80, 125, 0.86);
  border: 1px solid #00c8ff;
}

#TBtn_RemoveWarning {
  color: #fff4f4;
  background: rgba(128, 24, 24, 0.92);
  border: 1px solid #ff9a9a;
  border-radius: 8px;
  padding: 3px 8px;
  min-height: 36px;
}
#TBtn_RemoveWarning:hover {
  background: rgba(176, 35, 35, 0.96);
  border: 1px solid #ffd6d6;
}

#groupBox QToolButton:checked {
  background: rgba(0, 130, 200, 0.95);
  border: 1px solid #9fe7ff;
  color: #ffffff;
}
"""

tree.write('mainwindow.ui', encoding='utf-8', xml_declaration=True)
