// ПЧ-1500 — корпус для 3D-печати (OpenSCAD). Все размеры в мм.
// Детали: part = "base" (основание), "lid" (крышка с пультом), "leg" (ножка-проставка, 4 шт.), "all" (сборка для просмотра)
// Экспорт:  openscad -D 'part="base"' -o stl/base.stl pch1500_case.scad
part = "all";

// ---- основные параметры (подгоняются под купленные изделия) ----
W = 210; H = 170;            // наружные ширина и высота
wall = 3;
base_d = 60;                 // внутренняя глубина основания (над задней стенкой)
lid_d = 22;                  // внутренняя глубина крышки
hs_w = 200; hs_h = 100;      // радиатор (плоская сторона прилегает к задней стенке снаружи)
hs_overlap = 5;              // перекрытие радиатора и стенки по периметру окна
win_y = 19;                  // смещение центра окна/радиатора по вертикали
pcb_w = 160; pcb_h = 110;    // плата A1
pcb_x = -20;                 // смещение центра платы (справа на радиаторе остаётся место для моста VD1)
pcb_hole_inset = 4;
module_h = 5.6;              // высота корпуса модуля U1 от радиатора до нижней стороны платы — УТОЧНИТЬ ПО МОДУЛЮ
post_h = module_h - wall;    // высота стоек платы над внутренней поверхностью стенки
lcd_win = [97.5, 40];        // окно ЖКИ 2004
lcd_holes = [93, 55];        // крепёжные отверстия модуля ЖКИ
lcd_y = 38;
btn_y = -32; btn_pitch = 22; btn_d = 12.6;   // 5 кнопок 12x12 с круглыми колпачками
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
        translate([0, win_y, -wall - 1]) rbox(hs_w - 2 * hs_overlap, hs_h - 2 * hs_overlap, wall + 2, 2);
        // винты М4 впотай крепления радиатора (в радиаторе нарезать резьбу М4)
        for (sx = [-70, 0, 70], sy = [-1, 1]) translate([sx, win_y + sy * (hs_h / 2 - 2.5), -wall - 0.1]) {
            cylinder(d = 4.4, h = wall + 1); translate([0, 0, wall - 2.2]) cylinder(d1 = 4.4, d2 = 8.6, h = 2.4);
        }
        for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 85, sy * (H / 2 + 9), -wall - 1]) cylinder(d = 4.6, h = wall + 2);
        // кабельные вводы в нижней стенке: 3 x PG11 (сеть, двигатель, тормозной резистор), 2 x PG7 (управление, RS-485)
        for (x = [-70, -30, 10]) translate([x, -H / 2, 32]) rotate([90, 0, 0]) cylinder(d = 18.8, h = 3 * wall, center = true);
        for (x = [50, 78]) translate([x, -H / 2, 32]) rotate([90, 0, 0]) cylinder(d = 12.7, h = 3 * wall, center = true);
        // вентиляционные щели: верхняя стенка и боковые стенки
        translate([0, H / 2, 32]) vents(40, 22, 8);
        for (sx = [-1, 1]) translate([sx * W / 2, 10, 32]) rotate([0, 0, 90]) vents(40, 14, 8);
    }
    // стойки платы (саморез 2,9 мм)
    for (sx = [-1, 1], sy = [-1, 1])
        translate([pcb_x + sx * (pcb_w / 2 - pcb_hole_inset), win_y + sy * (pcb_h / 2 - pcb_hole_inset), 0])
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
            translate([0, lcd_y + 27]) text("ПЧ-1500   ~220 В → 3~220 В   1,5 кВт", size = 5, halign = "center", font = "DejaVu Sans:style=Bold");
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
    color("#9aa0a6") translate([0, win_y, -wall - 25]) linear_extrude(25) square([hs_w, hs_h], center = true);       // радиатор
    color("#2e7d32") translate([pcb_x, win_y, post_h]) linear_extrude(1.6) square([pcb_w, pcb_h], center = true);  // плата
}
