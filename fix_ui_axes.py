import re

with open("mainwindow.ui", "r", encoding="utf-8") as f:
    content = f.read()

target = """             <widget class="QToolButton" name="btnStepTargetSixAxis5">
              <property name="sizePolicy">
               <sizepolicy hsizetype="Preferred" vsizetype="Preferred">
                <horstretch>0</horstretch>
                <verstretch>4</verstretch>
               </sizepolicy>
              </property>
              <property name="text">
               <string>轴5</string>
              </property>
              <property name="checkable">
               <bool>true</bool>
              </property>
             </widget>
            </item>"""

replacement = target + """
            <item>
             <widget class="QToolButton" name="btnStepTargetSixAxis6">
              <property name="sizePolicy">
               <sizepolicy hsizetype="Preferred" vsizetype="Preferred">
                <horstretch>0</horstretch>
                <verstretch>4</verstretch>
               </sizepolicy>
              </property>
              <property name="text">
               <string>轴6</string>
              </property>
              <property name="checkable">
               <bool>true</bool>
              </property>
             </widget>
            </item>
            <item>
             <widget class="QToolButton" name="btnStepTargetCable">
              <property name="sizePolicy">
               <sizepolicy hsizetype="Preferred" vsizetype="Preferred">
                <horstretch>0</horstretch>
                <verstretch>4</verstretch>
               </sizepolicy>
              </property>
              <property name="text">
               <string>钢缆</string>
              </property>
              <property name="checkable">
               <bool>true</bool>
              </property>
             </widget>
            </item>"""

if target in content:
    content = content.replace(target, replacement)
    with open("mainwindow.ui", "w", encoding="utf-8") as f:
        f.write(content)
    print("Success")
else:
    print("Target not found")
