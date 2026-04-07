with open('mainwindow.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace Jog/Step unselected
# from: applyModeStyle(ui->TBtn_Stepmove, "rgba(70, 70, 85, 0.78)", "#7f8c98");
# to: applyModeStyle(ui->TBtn_Stepmove, "rgba(80, 80, 80, 0.78)", "#7f8c98");

new_stepmove_unknown = '        applyModeStyle(ui->TBtn_Stepmove, "rgba(100, 100, 100, 0.78)", "#7f8c98");'
new_movemode_unknown = '        applyModeStyle(ui->TBtn_MoveMode, "rgba(100, 100, 100, 0.78)", "#7f8c98");'

content = content.replace('applyModeStyle(ui->TBtn_Stepmove, "rgba(70, 70, 85, 0.78)", "#7f8c98");', new_stepmove_unknown)
content = content.replace('applyModeStyle(ui->TBtn_MoveMode, "rgba(70, 70, 85, 0.78)", "#7f8c98");', new_movemode_unknown)

# Replace Jog/Step colors
# step enabled ("rgba(0, 132, 255, 0.88)") -> dark purple? ("rgba(138, 43, 226, 0.88)")
# step disabled ("rgba(0, 88, 170, 0.86)") -> stay blue ("rgba(0, 132, 255, 0.88)")
# we'll make them distinct: Step = Purple (#a55eea), Jog = Blue (#00a8ff)

content = content.replace('applyModeStyle(ui->TBtn_Stepmove, "rgba(0, 132, 255, 0.88)", "#8ed8ff");', 'applyModeStyle(ui->TBtn_Stepmove, "rgba(156, 39, 176, 0.88)", "#d080ff");')
content = content.replace('applyModeStyle(ui->TBtn_Stepmove, "rgba(0, 88, 170, 0.86)", "#72c8ff");', 'applyModeStyle(ui->TBtn_Stepmove, "rgba(0, 132, 255, 0.88)", "#8ed8ff");')

with open('mainwindow.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
