// PUFVFD — корпус для 3D-печати (OpenSCAD). Все размеры в мм.
// Детали: part = "base" (основание), "lid" (крышка с пультом), "leg" (ножка-проставка, 4 шт.), "all" (сборка для просмотра)
// Экспорт:  openscad -D 'part="base"' -o stl/base.stl pch1500_case.scad
part = "all";

// ---- основные параметры (подгоняются под купленные изделия) ----
W = 210; H = 200;            // наружные ширина и высота
wall = 3;
base_d = 62;                 // внутренняя глубина основания (над задней стенкой)
lid_d = 22;                  // внутренняя глубина крышки
pcb_w = 180; pcb_h = 170;    // плата PUFVFD.01.000
pcb_top = 92;                // координата верхнего края платы (от центра корпуса)
// радиатор 125x150 мм (плоская сторона прилегает к задней стенке снаружи); под ним — модуль DA1 и мост VD1
hs_w = 125; hs_h = 150;
hs_x = -33.5; hs_y = 20;     // центр радиатора
hs_overlap = 5;              // перекрытие радиатора и стенки по периметру окна
module_h = 6.0;              // расстояние от радиатора до нижней стороны платы — ИЗМЕРИТЬ ПО УСТАНОВЛЕННОМУ МОДУЛЮ
post_h = module_h - wall;    // высота стоек платы над внутренней поверхностью стенки
// крепёжные отверстия платы (координаты платы: x вправо, y вниз от левого верхнего угла)
pcb_holes = [[4.5, 145.0], [46.0, 146.2], [114.0, 146.6], [176.0, 146.5], [176.0, 34.0], [4.5, 166.5]];   // седьмое (40,5; 35) — стойка М3 в радиатор
lcd_win = [97.5, 40];        // окно ЖКИ 2004
lcd_holes = [93, 55];        // крепёжные отверстия модуля ЖКИ
lcd_y = 45;
btn_y = -30; btn_pitch = 22; btn_d = 12.6;   // 5 кнопок 12x12 с круглыми колпачками
pot_d = 7.4;                 // отверстие потенциометра
leg_h = 50;                  // высота ножек (радиатор 25 мм + вентилятор 10 мм + зазор)
$fn = 40;

iw = W - 2 * wall; ih = H - 2 * wall;
cx = W / 2 - wall - 4.5; cy = H / 2 - wall - 4.5;      // оси винтов крышки
module rbox(w, h, d, r = 4) { linear_extrude(d) offset(r) square([w - 2 * r, h - 2 * r], center = true); }

module vents(len, n, pitch) { for (i = [0 : n - 1]) translate([(i - (n - 1) / 2) * pitch, 0, 0]) cube([3, 20, len], center = true); }

module base() {
    difference() {
        union() {
            translate([0, 0, -wall]) rbox(W, H, wall + base_d);
            // проушины крепления к стене (через ножки)
            for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 + 6), -wall]) rbox(18, 20, wall, 3);
        }
        rbox(iw, ih, base_d + 1, 2);                                   // внутренний объём
        // окно под радиатор
        translate([hs_x, hs_y, -wall - 1]) rbox(hs_w - 2 * hs_overlap, hs_h - 2 * hs_overlap, wall + 2, 2);
        // винты М4 впотай крепления радиатора (в радиаторе нарезать резьбу М4)
        for (sx = [-1, 1], sy = [-60, 0, 60]) translate([hs_x + sx * (hs_w / 2 - 2.5), hs_y + sy, -wall - 0.1]) {
            cylinder(d = 4.4, h = wall + 1); translate([0, 0, wall - 2.2]) cylinder(d1 = 4.4, d2 = 8.6, h = 2.4);
        }
        for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 + 9), -wall - 1]) cylinder(d = 4.6, h = wall + 2);
        // кабельные вводы в нижней стенке: 3 x PG11 (сеть, двигатель, тормозной резистор), 2 x PG7 (управление, RS-485)
        for (x = [-70, -30, 10]) translate([x, -H / 2, 32]) rotate([90, 0, 0]) cylinder(d = 18.8, h = 3 * wall, center = true);
        for (x = [50, 78]) translate([x, -H / 2, 32]) rotate([90, 0, 0]) cylinder(d = 12.7, h = 3 * wall, center = true);
        // вентиляционные щели: верхняя стенка и боковые стенки
        translate([0, H / 2, 32]) vents(40, 22, 8);
        for (sx = [-1, 1]) translate([sx * W / 2, 10, 32]) rotate([0, 0, 90]) vents(40, 17, 8);
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
        // надписи (гравировка 0,6 мм)
        translate([0, 0, lid_d + wall - 0.6]) linear_extrude(1) {
            labels = ["ПУСК", "СТОП", "▲", "▼", "ВВОД"];
            for (i = [-2 : 2]) translate([i * btn_pitch - 15, btn_y - 14]) text(labels[i + 2], size = 4.5, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([82, btn_y - 14]) text("Гц", size = 4.5, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([0, lcd_y + 27]) text("PUFVFD   ~220 В → 3~220 В   1,5 кВт", size = 5, halign = "center", font = "DejaVu Sans:style=Bold");
            translate([0, -H / 2 + 8]) text("ОПАСНО! После отключения ждать 5 минут", size = 4.2, halign = "center", font = "DejaVu Sans:style=Bold");
        }
    }
    // стойки ЖКИ (саморез 2,9 мм), высота задаёт положение стекла вровень с окном
    for (sx = [-1, 1], sy = [-1, 1]) translate([sx * lcd_holes[0] / 2, lcd_y + sy * lcd_holes[1] / 2, lid_d - 9])
        difference() { cylinder(d = 7, h = 9); translate([0, 0, -1]) cylinder(d = 2.5, h = 8); }
    // стойки платы кнопок (макетная плата 120x30, отверстия 110 мм)
    for (sx = [-1, 1]) translate([sx * 55 - 15, btn_y + 12, lid_d - 7]) difference() { cylinder(d = 7, h = 7); translate([0, 0, -1]) cylinder(d = 2.5, h = 6); }
    // ложемент модуля ESP32 (крепление двусторонним скотчем или стяжкой)
    translate([62, btn_y - 30, lid_d - 2]) difference() { cube([56, 32, 4], center = true); cube([52.5, 28.5, 5], center = true); }
}

module leg() { difference() { rbox(18, 20, leg_h, 3); translate([0, 3, -1]) cylinder(d = 4.6, h = leg_h + 2); } }

if (part == "base") base();
else if (part == "lid") rotate([180, 0, 0]) translate([0, 0, -lid_d - wall]) lid();   // печать лицевой стороной вниз
else if (part == "leg") leg();
else {
    color("#c8ccd0") base();
    color("#e9e2c8", 0.9) translate([0, 0, base_d + 30]) lid();
    color("#555") for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 + 6), -wall - leg_h]) leg();
    color("#9aa0a6") translate([hs_x, hs_y, -wall - 25]) linear_extrude(25) square([hs_w, hs_h], center = true);   // радиатор
    color("#2e7d32") translate([0, pcb_top - pcb_h / 2, post_h]) linear_extrude(1.6) difference() {                   // плата с вырезом под мост
        square([pcb_w, pcb_h], center = true); translate([-pcb_w / 2 - 1, pcb_h / 2 - 36]) square([35, 37]); }
}
