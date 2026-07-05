create database apresentacao;
\c apresentacao

create table departamento (cod integer primary key, nome varchar(30), orcamento double);
create table funcionario (id integer primary key, nome varchar(30), idade integer, salario double, depto integer references departamento(cod));

\d

insert into departamento values (1, 'engenharia', 500000.0);
insert into departamento values (2, 'vendas', 250000.0);
insert into departamento values (3, 'rh', 100000.0);
insert into funcionario values (1, 'func_1', 21, 1100.50, 2);
insert into funcionario values (2, 'func_2', 22, 1200.50, 3);
insert into funcionario values (3, 'func_3', 23, 1300.50, 1);
insert into funcionario values (4, 'func_4', 24, 1400.50, 2);
insert into funcionario values (5, 'func_5', 25, 1500.50, 3);
insert into funcionario values (6, 'func_6', 26, 1600.50, 1);
insert into funcionario values (7, 'func_7', 27, 1700.50, 2);
insert into funcionario values (8, 'func_8', 28, 1800.50, 3);
insert into funcionario values (9, 'func_9', 29, 1900.50, 1);
insert into funcionario values (10, 'func_10', 30, 2000.50, 2);
insert into funcionario values (11, 'func_11', 31, 2100.50, 3);
insert into funcionario values (12, 'func_12', 32, 2200.50, 1);
insert into funcionario values (13, 'func_13', 33, 2300.50, 2);
insert into funcionario values (14, 'func_14', 34, 2400.50, 3);
insert into funcionario values (15, 'func_15', 35, 2500.50, 1);
insert into funcionario values (16, 'func_16', 36, 2600.50, 2);
insert into funcionario values (17, 'func_17', 37, 2700.50, 3);
insert into funcionario values (18, 'func_18', 38, 2800.50, 1);
insert into funcionario values (19, 'func_19', 39, 2900.50, 2);
insert into funcionario values (20, 'func_20', 40, 3000.50, 3);
insert into funcionario values (21, 'func_21', 41, 3100.50, 1);
insert into funcionario values (22, 'func_22', 42, 3200.50, 2);
insert into funcionario values (23, 'func_23', 43, 3300.50, 3);
insert into funcionario values (24, 'func_24', 44, 3400.50, 1);
insert into funcionario values (25, 'func_25', 45, 3500.50, 2);
insert into funcionario values (26, 'func_26', 46, 3600.50, 3);
insert into funcionario values (27, 'func_27', 47, 3700.50, 1);
insert into funcionario values (28, 'func_28', 48, 3800.50, 2);
insert into funcionario values (29, 'func_29', 49, 3900.50, 3);
insert into funcionario values (30, 'func_30', 50, 4000.50, 1);
insert into funcionario values (31, 'func_31', 51, 4100.50, 2);
insert into funcionario values (32, 'func_32', 52, 4200.50, 3);
insert into funcionario values (33, 'func_33', 53, 4300.50, 1);
insert into funcionario values (34, 'func_34', 54, 4400.50, 2);
insert into funcionario values (35, 'func_35', 55, 4500.50, 3);
insert into funcionario values (36, 'func_36', 56, 4600.50, 1);
insert into funcionario values (37, 'func_37', 57, 4700.50, 2);
insert into funcionario values (38, 'func_38', 58, 4800.50, 3);
insert into funcionario values (39, 'func_39', 59, 4900.50, 1);
insert into funcionario values (40, 'func_40', 20, 5000.50, 2);
insert into funcionario values (41, 'func_41', 21, 5100.50, 3);
insert into funcionario values (42, 'func_42', 22, 5200.50, 1);
insert into funcionario values (43, 'func_43', 23, 5300.50, 2);
insert into funcionario values (44, 'func_44', 24, 5400.50, 3);
insert into funcionario values (45, 'func_45', 25, 5500.50, 1);
insert into funcionario values (46, 'func_46', 26, 5600.50, 2);
insert into funcionario values (47, 'func_47', 27, 5700.50, 3);
insert into funcionario values (48, 'func_48', 28, 5800.50, 1);
insert into funcionario values (49, 'func_49', 29, 5900.50, 2);
insert into funcionario values (50, 'func_50', 30, 6000.50, 3);

select * from departamento;
select * from funcionario;
select nome, salario from funcionario where idade > 50;
select * from funcionario where id = 42;

insert into funcionario values (1, 'pk_duplicada', 30, 1.0, 1);
insert into funcionario values (999, 'fk_invalida', 30, 1.0, 77);

update funcionario set salario = 9999.99 where id = 10;
select * from funcionario where id = 10;
delete from funcionario where id = 20;
select * from funcionario where id = 20;

exit
