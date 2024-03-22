# Serial port Plotter 1.0 by Solderingironspb
Данная программа предназначена для отрисовки графиков, используя данные с микроконтроллера, при отладке приложений.  Для работы требуется связь по UART.
# Возможности:
* Выводить графики в реалтайме
* Ручное и автоматическое масштабирование
* Выбор: бегущий график или статическая картинка
* Вывод отладочной информации (данные по каналам)
* Возможность скрывать ненужные элементы программы
* Возможность делать программу поверх всех окон
* Возможность убрать рамку программы
* Сохранение данных в *.csv файл (в корень программы)
* Сохранение снимка графика в *.png (в корень программы)
* Поддержка OpenGL(В зависимости от железа. На очень старых видеокартах может не работать)
* Программа сохраняет настройки окон, расположение окон и настройки подключения к COM порту
  
# Начальное окно программы
![1](https://github.com/Solderingironspb/Serial_port_Plotter_by_Solderingironspb/assets/68805120/2110a826-41a1-4e22-99d8-be3e1d777a66)
# Рисование графиков
![2](https://github.com/Solderingironspb/Serial_port_Plotter_by_Solderingironspb/assets/68805120/4238d7bf-d9b3-4c0e-974c-3ee5c105e8ad)
# Возможность закрепить график поверх всех окон + сделать безрамочный режим
![3](https://github.com/Solderingironspb/Serial_port_Plotter_by_Solderingironspb/assets/68805120/ab41f0ed-7f1d-437f-89a3-272332c13f8e)
 
 ## О Serial port Plotter by Solderingiron:
 Данная программа предназначена для отрисовки графиков, используя данные с микроконтроллера, при отладке приложений.
 Для работы требуется связь по UART.
 С микроконтроллера должны лететь char массивы с числовыми данными.
 Начало сообщения начинается с символа '$', далее идет число в обычном формате,
 например '3.14159'. Разделяются числа пробелом. Заканчивается посылка символом ';'
 
 ## Чтоб было совсем понятно, вот пример передачи с микроконтроллера одного числа для отладки:
 $3.14159;
 ## А вот передача четырех чисел:
 $3.14159 60 125.625 14.259;
 ## Важно:

 ``` 
Нужно данные слать каждый раз в одинаковом количестве.
Если начали отправлять, допустим 5 переменных, то и отправляйте по 5 переменных.
Ни больше, ни меньше.
```

 ## Для быстрой работы написал библиотеку на Си для микроконтроллеров.
 
 ```
 В папке с программой есть папка Solderingiron_libs. В ней файлы.
```
```
P.S. соверую использовать USB-TTL конвертер на чипе CH340.
При тестах он показал себя стабильней, чем тот же USB CDC в ST-Link v2.1.
Тот порядком сбоил при подключениях программы, или же, если на исследуемой плате нажать ресет.
```

### [Скачать программу (win_x64)](https://github.com/Solderingironspb/Serial_port_Plotter_by_Solderingironspb/releases/download/v1.0/Serial_port_Plotter_v1.0_win_x64.zip)



 
