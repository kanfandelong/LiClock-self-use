pi = 0
a = 0
b = 0
gui.drawWindowsWithTitle('布法罗公式π计算', 0, 0, 296, 128)
j = gui.msgbox_number('计算迭代次数',5, 100)
for i = 0, 1000, 1 do
  t = 1 / 16 ^ i
  my_8t = i * 8
  t1 = t * (4 / (my_8t + 1))
  t2 = t * (2 / (my_8t + 4))
  t3 = t * (1 / (my_8t + 5))
  t4 = t * (1 / (my_8t + 6))
  pi = pi + (t1 - (t2 + (t3 + t4)))
  if pi ~= b then
    display.fillRect(2, 15, (14 * 14), 30, 1)
    display.setCursor(2, 30)
    display.u8g2Print(('中间结果，π：' .. pi))
    display.setCursor(2, 44)
    display.u8g2Print(('迭代次数：' .. i))
    if a == 15 then
      display.display(false)
      a = 0
    else
      display.display(true)
      a = a + 1
    end
  end
  b = pi
end
display.fillRect(2, 15, 98, 30, 1)
display.setCursor(2, 30)
display.u8g2Print(('计算结束，π：' .. pi))
display.display(false)
