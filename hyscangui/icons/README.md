Иконки являются частью набора [Google Material Design Monochrome][1], структура 
папок согласно теме hicolor.

Преобразовать svg-иконки в symbolic можно с помощью утилиты gtk-encode-symbolic-svg:

```bash
find scalable/actions -name '*.svg' -exec gtk-encode-symbolic-svg -o 16x16/actions {} 16x16 \;
```

[1]:https://www.flaticon.com/authors/google-material-design/monochrome

Updated 06.10.2021, Andrey Zakharov <zaharov@screen-co.ru>]
Добавлены иконки для Журнала Меток - папка "mark-manager".
