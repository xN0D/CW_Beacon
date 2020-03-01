_**Маяк на основе телеграфного трансивера Super RM или Super Octopus или RockMite51. 
Разработал Игорь Кутко, UA3YMC, 1 марта 2020г.**_

Изначальной причиной, по которой мне пришлось написать этот код, стало то, 
что оригинальный функционал трансивера был крайне отвратительным.
При неплохой аппаратной реализации программное обеспечение было просто ужасным.
У телеграфного ключа трансивера отсутствовали повторы точек и тире, что делало практически невозможным работу 
телеграфом в ручном режиме. Работа с компьютера была возможна только через специализированное ПО которое, 
ко всему, было на китайском языке. Данные обстоятельства и побудили меня к написанию данного кода.

В оригинале использовался микроконтроллер ST12C4052AD, но мне не удалось найти для него
необходимой документации, поэтому я решил заменить его на другой, более удобный для меня, микроконтроллер ATTiny4313.
Данный микроконтроллер практически полностью совпадает по пинам с ST12C4052AD, но RESET у него 
осуществляется по другому, поэтому в схему трансивера необходимо внести небольшие изменения, 
удалив с платы конденсатор Е1 и резистор R27.

Так же, ввиду того, что я не смог найти способ напрямую управлять портом B0(pin 12), мне пришлось сделать
аппаратный костыль, соединив на плате 9 и 12 ножки микроконтроллера, и производить управление портом D5 (pin 9).
Данный костыль может оказаться полезным для отключения передачи в эфир, если сделать перемычку через дополнительный тумблер.

Функционал:
В данной прошивке реализован автоматический телеграфный ключ с регулировкой тона и скорости передачи.
Ключ имеет 8 тонов и 10 скоростей. Частота тона выбиралась из соображений благозвучности и возможности 
комфортного и длительного восприятия. Диапазон скоростей ориентирован на среднестатистического радиолюбителя.
В режиме маяка трансивер автоматически передает сообщение заранее записанное в EEPROM микроконтроллера.
Запуск и останов режима маяка осуществляются кнопкой с передней панели или отправкой символа '~' в режиме терминала. В новой версии прошивки запись сообщений в EEPROM осуществляется посредством терминала. ATTiny4313 имеет 256 байт встроенной EEPROM памяти, что хватает на сообщение в 255 символов, но в контроллере очень мало оперативной памяти, поэтому запись всех 256 байт сообщения за один раз затруднительна. Для удобства я решил записывать сообщения в EEPROM по частям. Для этого я виртуально разбил все пространство EEPROM на 4 части с индексами с 0 по 3. Для записи сообщения необходимо ввести в терминал (или вставить из буфера обмена заранее приготовленное) в следующем порядке:

номер_части_eeprom сообщение_максимально_64_символа в_конце_символ_'^'

Все слитно без пробелов и лишних символов. 
Пример:

_0TEST MESSAGE^_

При такой команде в первую часть EEPROM запишется сообщение:

_TEST MESSAGE_ 

Первый символ всегда должен быть цифрой от 0 до 3, последним всегда ставится символ '^'. В любом другом случае в терминате появится сообщение "ERR".
Если длинна всего сообщения больше 64 символов то не влезший остаток нужно перенести в следующую часть EEPROM

_1TEST MESSAGE^_

_2TEST MESSAGE^_

_3TEST MESSAGE^_

При передаче маяк будет считывать весь EEPROM до символа окончания FF(255) который ставится автоматически вместо '^' в сообщении. Нужно учитывать, что если сообщение в первой части EEPROM менее 64 символов то это значит, что после него обязательно будет стоять знак окончания, и маяк не будет считывать сообщения в остальных частях EEPROM.

Перечень передаваемых символов можно посмотреть в коде, в функции CodeMorse.
В режиме терминала трансивер передает сообщения, полученные через RS232 порт, что позволяет его использовать без 
дополнительного ПО на любой операционной системе. Передача сообщений происходит построчно строками длинной до 67 символов 
верхнего или нижнего регистра. При передаче строки текста трансивер возвращает эхом принятую строку и, по окончанию передачи строки в эфир, выводит в терминал сообщение "ОК". В режиме маяка по окончанию передачи и паузы между передачами, в терминал будут поступать сообщения вида "QRV".

Скорость порта RS232, а так же режим скорости передачи по умолчанию, тон по умолчанию, паузу между повторами, можно изменить по своему усмотрению в коде, ищите соответствующие комментарии.

Код написан в среде Arduino IDE v 1.8.12
Использовано ядро ATTinyCore by Spence Konde
Для установки ядра необходимо сделать следующее:
Зайти в Arduino IDE: Файл -> Настройки -> Дополнительные ссылки для менеджера плат. 
Ввести http://drazzy.com/package_drazzy.com_index.json
Зайти: Инструменты -> Плата -> Менеджер плат. В строке поиска ввести ATTinyCore
Найти ATTinyCore by Spence Konde и установить.
Перед прошивкой необходимо сконфигурировать фьюзы микроконтроллера.
Конфигурация фьюзов (E:FF, H:DF, L:EF)

_avrdude -p t4313 -P /dev/ttyUSB0 -c avrisp -b 19200 -U lfuse:w:0xEF:m  -U hfuse:w:0xDF:m  -U efuse:w:0xFF:m_

Компиляцию производить со следующими параметрами:

Плата 	ATTiny2313/4313

Chip 	4313

Clock 	11.0592

Все остальное по умолчанию.
