// PUFVFD — корпус для 3D-печати (OpenSCAD). Все размеры в мм.
// Детали (part):
//   цельные (стол принтера не меньше 235 × 215 мм): "base" — основание, "lid" — крышка с пультом;
//   разрезные (стол 220 × 220 мм): "base_a", "base_b", "lid_a", "lid_b" — половинки, стягиваются пластинами
//   "splice_wall" (2 шт.), "splice_back" (1 шт.), "splice_lid" (4 шт.) и винтами М3;
//   общие: "ear" — ушко крепления к стене (4 шт.), "leg" — ножка-проставка (4 шт.); "all" — сборка для просмотра.
// Все детали выводятся уже развёрнутыми для печати. Сборка STL: enclosure/build_stl.sh
part = "all";

// ---- основные параметры (подгоняются под купленные изделия) ----
W = 210; H = 230;            // наружные ширина и высота (плата 180 × 170 + место для ввода кабелей снизу)
wall = 3;
base_d = 62;                 // внутренняя глубина основания (над задней стенкой)
lid_d = 22;                  // внутренняя глубина крышки
pcb_w = 180; pcb_h = 170;    // плата PUFVFD.01.000
pcb_top = 102;               // координата верхнего края платы (от центра корпуса); снизу остаётся 44 мм для кабелей
// радиатор 125x150 мм (плоская сторона прилегает к задней стенке снаружи); под ним — модуль DA1 и мост VD1
hs_w = 125; hs_h = 150;
hs_x = -33.5; hs_y = 25;     // центр радиатора (верхний край на 15 мм ниже края корпуса — место для ушек)
hs_overlap = 5;              // перекрытие радиатора и стенки по периметру окна
module_h = 6.0;              // расстояние от радиатора до нижней стороны платы — ИЗМЕРИТЬ ПО УСТАНОВЛЕННОМУ МОДУЛЮ
post_h = module_h - wall;    // высота стоек платы над внутренней поверхностью стенки
// крепёжные отверстия платы (координаты платы: x вправо, y вниз от левого верхнего угла)
pcb_holes = [[4.5, 145.0], [46.0, 146.2], [114.0, 146.6], [176.0, 146.5], [176.0, 34.0], [4.5, 166.5]];   // седьмое (40,5; 35) — стойка М3 в радиатор
lcd_win = [97.5, 40];        // окно ЖКИ 2004
lcd_holes = [93, 55];        // крепёжные отверстия модуля ЖКИ
lcd_y = 50;
btn_y = -26; btn_pitch = 22; btn_d = 12.6;   // 5 кнопок 12x12 с круглыми колпачками
pot_d = 7.4;                 // отверстие потенциометра
leg_h = 50;                  // высота ножек (радиатор 25 мм + вентилятор 10 мм + зазор)
split = (part == "base_a" || part == "base_b" || part == "lid_a" || part == "lid_b" || part == "all_split");
split_y = 0;                 // линия разреза половинок
engrave = true;              // надписи на крышке (для проверки посадки отключаются — ускоряет расчёт)
$fn = 40;

iw = W - 2 * wall; ih = H - 2 * wall;
cx = W / 2 - wall - 4.5; cy = H / 2 - wall - 4.5;      // оси винтов крышки
module rbox(w, h, d, r = 4) { linear_extrude(d) offset(r) square([w - 2 * r, h - 2 * r], center = true); }

// щели; skip — координата (вдоль ряда), около которой щелей нет (место стяжных пластин разрезного варианта)
module vents(len, n, pitch, skip = 1e9) {
    for (i = [0 : n - 1]) if (abs((i - (n - 1) / 2) * pitch - skip) > 13) translate([(i - (n - 1) / 2) * pitch, 0, 0]) cube([3, 20, len], center = true);
}
// каплевидное отверстие с горизонтальной осью вдоль Y
module teardrop(d, h) { rotate([90, 0, 0]) linear_extrude(h, center = true) union() { circle(d / 2); rotate(45) square(d / 2); } }

module base() {
    difference() {
        translate([0, 0, -wall]) rbox(W, H, wall + base_d);
        rbox(iw, ih, base_d + 1, 2);                                   // внутренний объём
        // окно под радиатор
        translate([hs_x, hs_y, -wall - 1]) rbox(hs_w - 2 * hs_overlap, hs_h - 2 * hs_overlap, wall + 2, 2);
        // винты М4 впотай крепления радиатора (в радиаторе нарезать резьбу М4)
        for (sx = [-1, 1], sy = [-60, 0, 60]) translate([hs_x + sx * (hs_w / 2 - 2.5), hs_y + sy, -wall - 0.1]) {
            cylinder(d = 4.4, h = wall + 1); translate([0, 0, wall - 2.2]) cylinder(d1 = 4.4, d2 = 8.6, h = 2.4);
        }
        // крепление ушек (ear): по два винта М3 в каждом углу, головки внутри, гайки в ушке
        for (sx = [-1, 1], sy = [-1, 1], dx = [-5, 5]) translate([sx * 85 + dx, sy * (H / 2 - 6.5), -wall - 1]) cylinder(d = 3.4, h = wall + 2);
        // разрезной вариант: отверстия под стяжные пластины
        if (split) {
            for (sx = [-1, 1], dy = [-7, 7], z = [15, 47]) translate([sx * W / 2, split_y + dy, z]) rotate([0, sx * 90, 0]) {
                cylinder(d = 3.4, h = 2 * wall + 2, center = true);
                translate([0, 0, -wall / 2 - 1.01 + (wall + 2) / 2]) cylinder(d1 = 3.4, d2 = 7.0, h = 1.9);   // потай снаружи
            }
            for (x = [45, 85], dy = [-7, 7]) translate([x, split_y + dy, -wall - 1]) {
                cylinder(d = 3.4, h = wall + 2); translate([0, 0, wall + 1 - 1.9]) cylinder(d1 = 3.4, d2 = 7.0, h = 1.91);   // потай внутри
            }
        }
        // кабельные вводы в нижней стенке: 3 x PG11 (сеть, двигатель, тормозной резистор), 2 x PG7 (управление, RS-485)
        // отверстия каплевидные — печатаются без поддержек (вершина капли закрывается фланцем ввода)
        for (x = [-70, -30, 10]) translate([x, -H / 2, 32]) teardrop(18.8, 3 * wall);
        for (x = [50, 78]) translate([x, -H / 2, 32]) teardrop(12.7, 3 * wall);
        // вентиляционные щели: верхняя стенка и боковые стенки
        translate([0, H / 2, 32]) vents(40, 22, 8);
        for (sx = [-1, 1]) translate([sx * W / 2, 10, 32]) rotate([0, 0, 90]) vents(40, 17, 8, skip = split_y - 10);
    }
    // стойки платы (саморез 2,9 мм)
    for (p = pcb_holes)
        translate([p[0] - pcb_w / 2, pcb_top - p[1], 0])
            difference() { cylinder(d = 8, h = post_h); translate([0, 0, -wall + 0.6]) cylinder(d = 2.5, h = post_h + wall); }
    // угловые бобышки под винты крышки (саморез 2,9 мм или втулка М3)
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cx, sy * cy, 0])
        difference() { translate([-4.5, -4.5, 0]) cube([9, 9, base_d]); translate([0, 0, base_d - 18]) cylinder(d = 2.6, h = 19); }
}

module lid() {
    difference() {
        union() {
            rbox(W, H, wall + lid_d);
            translate([0, 0, -2]) difference() { rbox(iw - 0.6, ih - 0.6, 2.1, 2); rbox(iw - 4.6, ih - 4.6, 5, 2);
                for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cx, sy * cy, 0]) cube([11, 11, 12], center = true); }   // центрирующий буртик
        }
        translate([0, 0, -0.1]) rbox(iw, ih, lid_d + 0.1, 2);
        translate([0, lcd_y, lid_d + wall / 2]) cube([lcd_win[0], lcd_win[1], wall + 3], center = true);
        for (i = [-2 : 2]) translate([i * btn_pitch - 15, btn_y, lid_d - 1]) cylinder(d = btn_d, h = wall + 3);
        translate([82, btn_y, lid_d - 1]) cylinder(d = pot_d, h = wall + 3);
        for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cx, sy * cy, -1]) { cylinder(d = 3.4, h = lid_d + wall + 2); translate([0, 0, lid_d + wall - 1.8 + 1]) cylinder(d1 = 3.4, d2 = 6.8, h = 1.9); }
        if (split) {                                      // разрезной вариант: отверстия под стяжные пластины
            for (sx = [-1, 1], dy = [-7, 7], z = [6, 16]) translate([sx * W / 2, split_y + dy, z]) rotate([0, sx * 90, 0]) {
                cylinder(d = 3.4, h = 2 * wall + 2, center = true);
                translate([0, 0, -wall / 2 - 1.01 + (wall + 2) / 2]) cylinder(d1 = 3.4, d2 = 7.0, h = 1.9);
            }
            for (x = [-88, 70], dx = [-5, 5], dy = [-7, 7]) translate([x + dx, split_y + dy, lid_d - 1]) {
                cylinder(d = 3.4, h = wall + 2); translate([0, 0, wall + 1 - 1.9 + 0.01]) cylinder(d1 = 3.4, d2 = 7.0, h = 1.91);
            }
        }
        // надписи (гравировка 0,6 мм)
        if (engrave) translate([0, 0, lid_d + wall - 0.6]) linear_extrude(1) {
            labels = ["ПУСК", "СТОП", "▲", "▼", "ВВОД"];
            for (i = [-2 : 2]) translate([i * btn_pitch - 15, btn_y - 14]) text(labels[i + 2], size = 4.5, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([82, btn_y - 14]) text("Гц", size = 4.5, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([0, lcd_y + 40]) text("PUFVFD", size = 12, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([0, lcd_y + 28.5]) text("ПРЕОБРАЗОВАТЕЛЬ ЧАСТОТЫ  1,5 кВт  ~220 В / 3~220 В", size = 4, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([0, -H / 2 + 8]) text("ОПАСНО! После отключения ждать 5 минут", size = 4.2, halign = "center", font = "DejaVu Sans:style=Bold");
        }
    }
    // угловые трубки под винты крепления крышки к основанию (опираются на бобышки основания)
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * cx, sy * cy, 0])
        difference() { cylinder(d = 8, h = lid_d); translate([0, 0, -1]) cylinder(d = 3.4, h = lid_d + 2); }
    // стойки ЖКИ (саморез 2,9 мм), высота задаёт положение стекла вровень с окном
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * lcd_holes[0] / 2, lcd_y + sy * lcd_holes[1] / 2, lid_d - 9])
        difference() { cylinder(d = 7, h = 9); translate([0, 0, -1]) cylinder(d = 2.5, h = 8); }
    // стойки платы кнопок (макетная плата 120x30, отверстия 110 мм)
    for (sx = [-1, 1]) translate([sx * 55 - 15, btn_y + 12, lid_d - 7]) difference() { cylinder(d = 7, h = 7); translate([0, 0, -1]) cylinder(d = 2.5, h = 6); }
    // ложемент модуля ESP32 (крепление двусторонним скотчем или стяжкой)
    translate([62, btn_y - 30, lid_d - 2]) difference() { cube([56, 32, 4], center = true); cube([52.5, 28.5, 5], center = true); }
}

module leg() { difference() { rbox(18, 20, leg_h, 3); translate([0, 3, -1]) cylinder(d = 4.6, h = leg_h + 2); } }

// ушко крепления к стене: перекрывает угол задней стенки на 9 мм (2 винта М3, гайки в ушке), выступает за корпус
module ear() {
    difference() {
        translate([0, 6, 0]) rbox(22, 25, 5, 3);
        for (dx = [-5, 5]) translate([dx, -2, -1]) { cylinder(d = 3.4, h = 7); cylinder(d = 6.4, h = 1 + 2.6, $fn = 6); }  // гайка М3 снизу
        translate([0, 12.5, -1]) cylinder(d = 4.6, h = 7);                              // винт или шуруп в стену (через ножку)
    }
}
// стяжная пластина: 4 отверстия М3 с гнёздами под гайки
module splice(w, h, t, holes) {
    difference() {
        translate([-w / 2, -h / 2, 0]) cube([w, h, t]);
        for (p = holes) translate([p[0], p[1], -1]) { cylinder(d = 3.4, h = t + 2); translate([0, 0, t + 1 - 2.6]) cylinder(d = 6.4, h = 3, $fn = 6); }
    }
}
module splice_wall() { splice(46, 26, 4, [[-16, -7], [-16, 7], [16, -7], [16, 7]]); }    // боковые стенки основания (по z: 15 и 47)
module splice_back() { splice(66, 26, 3, [[-20, -7], [-20, 7], [20, -7], [20, 7]]); }     // задняя стенка снаружи (x 45 и 85)
module splice_lid()  { splice(26, 26, 4, [[-5, -7], [-5, 7], [5, -7], [5, 7]]); }        // крышка: стенки (z 6 и 16) и лицевая панель

// половинка детали: полупространство y > split_y (a) или y < split_y (b)
module half(side) { intersection() { children(); translate([-200, side == "a" ? split_y : split_y - 400, -100]) cube([400, 400, 300]); } }

module lid_print() { rotate([180, 0, 0]) translate([0, 0, -lid_d - wall]) lid(); }   // лицевой стороной вниз

if (part == "base") translate([0, 0, wall]) base();                     // задней стенкой на стол
else if (part == "lid") lid_print();
else if (part == "base_a") translate([0, 0, wall]) half("a") base();
else if (part == "base_b") translate([0, 0, wall]) half("b") base();
else if (part == "lid_a") rotate([180, 0, 0]) translate([0, 0, -lid_d - wall]) half("a") lid();
else if (part == "lid_b") rotate([180, 0, 0]) translate([0, 0, -lid_d - wall]) half("b") lid();
else if (part == "leg") leg();
else if (part == "ear") ear();
else if (part == "splice_wall") splice_wall();
else if (part == "splice_back") splice_back();
else if (part == "splice_lid") splice_lid();
else {
    color("#c8ccd0") base();
    color("#e9e2c8", 0.9) translate([0, 0, base_d + 30]) lid();
    color("#555") for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 + 5), -wall - leg_h - 5]) mirror([0, sy < 0 ? 1 : 0, 0]) leg();
    color("#777") for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 - 4.5), -wall - 5]) mirror([0, sy < 0 ? 1 : 0, 0]) ear();
    color("#9aa0a6") translate([hs_x, hs_y, -wall - 25]) linear_extrude(25) square([hs_w, hs_h], center = true);   // радиатор
    color("#2e7d32") translate([0, pcb_top - pcb_h / 2, post_h]) linear_extrude(1.6) difference() {                   // плата с вырезом под мост
        square([pcb_w, pcb_h], center = true); translate([-pcb_w / 2 - 1, pcb_h / 2 - 36]) square([35, 37]); }
}
