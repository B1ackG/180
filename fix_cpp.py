with open('mainwindow.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find and replace the groupBox style block
start_str = """        if (ui->groupBox->title().isEmpty()) {
            ui->groupBox->setTitle("页面导航");
        }"""
        
end_str = """        ui->horizontalLayout_2->setContentsMargins(10, 6, 10, 6);"""
start_idx = content.find(start_str)
end_idx = content.find(end_str)

if start_idx != -1 and end_idx != -1:
    content = content[:start_idx + len(start_str)] + "\n" + content[end_idx:]

start_str2 = """        for (QToolButton *btn : navButtons) {
            if (!btn) {
                continue;
            }
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setIconSize(QSize(22, 22));"""

end_str2 = """        }
    }

    if (ui && ui->groupBox_4) {"""
start_idx2 = content.find(start_str2)
end_idx2 = content.find(end_str2)

if start_idx2 != -1 and end_idx2 != -1:
    content = content[:start_idx2 + len(start_str2)] + "\n" + content[end_idx2:]

start_str3 = """    if (ui && ui->groupBox_4) {
        if (ui->groupBox_4->title().isEmpty()) {
            ui->groupBox_4->setTitle("安全开关");
        }"""
        
end_str3 = """    }

    //record"""
start_idx3 = content.find(start_str3)
end_idx3 = content.find(end_str3)

if start_idx3 != -1 and end_idx3 != -1:
    content = content[:start_idx3 + len(start_str3)] + "\n" + content[end_idx3:]

with open('mainwindow.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
