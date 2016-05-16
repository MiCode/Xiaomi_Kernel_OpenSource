	.arch armv4t
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 18, 4
	.file	"gsl_point_id.c"
	.local	point_array
	.comm	point_array, 1200, 4
	.local	point_pointer
	.comm	point_pointer, 40, 4
	.local	point_stretch
	.comm	point_stretch, 40, 4
	.local	point_report
	.comm	point_report, 40, 4
	.local	point_now
	.comm	point_now, 40, 4
	.local	point_delay
	.comm	point_delay, 40, 4
	.local	filter_deep
	.comm	filter_deep, 40, 4
	.local	point_edge
	.comm	point_edge, 12, 4
	.local	point_decimal
	.comm	point_decimal, 40, 4
	.local	pressure_now
	.comm	pressure_now, 40, 4
	.local	pressure_array
	.comm	pressure_array, 320, 4
	.local	pressure_report
	.comm	pressure_report, 40, 4
	.local	pressure_pointer
	.comm	pressure_pointer, 32, 4
	.local	global_state
	.comm	global_state, 4, 4
	.local	inte_count
	.comm	inte_count, 4, 4
	.local	csensor_count
	.comm	csensor_count, 4, 4
	.local	point_n
	.comm	point_n, 4, 4
	.local	point_num
	.comm	point_num, 4, 4
	.local	prev_num
	.comm	prev_num, 4, 4
	.local	point_near
	.comm	point_near, 4, 4
	.local	point_shake
	.comm	point_shake, 4, 4
	.local	reset_mask_send
	.comm	reset_mask_send, 4, 4
	.local	reset_mask_max
	.comm	reset_mask_max, 4, 4
	.local	reset_mask_count
	.comm	reset_mask_count, 4, 4
	.local	global_flag
	.comm	global_flag, 4, 4
	.local	id_flag
	.comm	id_flag, 4, 4
	.local	id_first_coe
	.comm	id_first_coe, 4, 4
	.local	id_speed_coe
	.comm	id_speed_coe, 4, 4
	.local	id_static_coe
	.comm	id_static_coe, 4, 4
	.local	average
	.comm	average, 4, 4
	.local	soft_average
	.comm	soft_average, 4, 4
	.local	report_delay
	.comm	report_delay, 4, 4
	.local	delay_key
	.comm	delay_key, 4, 4
	.local	report_ahead
	.comm	report_ahead, 4, 4
	.local	report_delete
	.comm	report_delete, 4, 4
	.local	median_dis
	.comm	median_dis, 4, 4
	.local	shake_min
	.comm	shake_min, 4, 4
	.local	match_y
	.comm	match_y, 8, 4
	.local	match_x
	.comm	match_x, 8, 4
	.local	ignore_y
	.comm	ignore_y, 8, 4
	.local	ignore_x
	.comm	ignore_x, 8, 4
	.local	screen_y_max
	.comm	screen_y_max, 4, 4
	.local	screen_x_max
	.comm	screen_x_max, 4, 4
	.local	point_num_max
	.comm	point_num_max, 4, 4
	.local	drv_num
	.comm	drv_num, 4, 4
	.local	sen_num
	.comm	sen_num, 4, 4
	.local	drv_num_nokey
	.comm	drv_num_nokey, 4, 4
	.local	sen_num_nokey
	.comm	sen_num_nokey, 4, 4
	.local	coordinate_correct_able
	.comm	coordinate_correct_able, 4, 4
	.local	coordinate_correct_coe_x
	.comm	coordinate_correct_coe_x, 256, 4
	.local	coordinate_correct_coe_y
	.comm	coordinate_correct_coe_y, 256, 4
	.local	edge_cut
	.comm	edge_cut, 16, 4
	.local	stretch_array
	.comm	stretch_array, 128, 4
	.local	stretch_active
	.comm	stretch_active, 128, 4
	.local	shake_all_array
	.comm	shake_all_array, 64, 4
	.local	edge_start
	.comm	edge_start, 4, 4
	.local	reset_mask_dis
	.comm	reset_mask_dis, 4, 4
	.local	reset_mask_type
	.comm	reset_mask_type, 4, 4
	.local	key_map_able
	.comm	key_map_able, 4, 4
	.local	key_range_array
	.comm	key_range_array, 96, 4
	.local	filter_able
	.comm	filter_able, 4, 4
	.local	filter_coe
	.comm	filter_coe, 16, 4
	.local	multi_x_array
	.comm	multi_x_array, 16, 4
	.local	multi_y_array
	.comm	multi_y_array, 16, 4
	.local	multi_group
	.comm	multi_group, 1024, 4
	.local	ps_coe
	.comm	ps_coe, 128, 4
	.local	pr_coe
	.comm	pr_coe, 128, 4
	.local	point_repeat
	.comm	point_repeat, 8, 4
	.local	near_set
	.comm	near_set, 8, 4
	.local	diagonal
	.comm	diagonal, 4, 4
	.local	point_extend
	.comm	point_extend, 4, 4
	.local	edge_first
	.comm	edge_first, 4, 4
	.local	edge_first_coe
	.comm	edge_first_coe, 4, 4
	.local	point_corner
	.comm	point_corner, 4, 4
	.local	config_static
	.comm	config_static, 2048, 4
	.local	gesture_num
	.comm	gesture_num, 4, 4
	.local	gesture_num_last
	.comm	gesture_num_last, 4, 4
	.local	gesture_dis_min
	.comm	gesture_dis_min, 4, 4
	.local	gesture_deal
	.comm	gesture_deal, 4, 4
	.local	gesture_last
	.comm	gesture_last, 4, 4
	.local	gesture_value
	.comm	gesture_value, 4, 4
	.local	gesture_threshold
	.comm	gesture_threshold, 8, 4
	.local	x_scale
	.comm	x_scale, 4, 4
	.local	y_scale
	.comm	y_scale, 4, 4
	.local	click_count
	.comm	click_count, 16, 4
	.local	point_click
	.comm	point_click, 16, 4
	.local	double_click
	.comm	double_click, 4, 4
	.local	double_down
	.comm	double_down, 4, 4
	.local	double_up
	.comm	double_up, 4, 4
	.local	gesture_min_x
	.comm	gesture_min_x, 4, 4
	.local	gesture_min_y
	.comm	gesture_min_y, 4, 4
	.local	size_ratio
	.comm	size_ratio, 4, 4
	.local	multi_x
	.comm	multi_x, 40, 4
	.local	multi_o
	.comm	multi_o, 40, 4
	.local	gesture_multi
	.comm	gesture_multi, 4, 4
	.local	multi_set
	.comm	multi_set, 4, 4
	.local	gesture_flag
	.comm	gesture_flag, 4, 4
	.local	model_extern
	.comm	model_extern, 4, 4
	.local	model_extern_len
	.comm	model_extern_len, 4, 4
	.local	ReadIICInt
	.comm	ReadIICInt, 4, 4
	.section	.rodata
	.align	2
	.type	model_default, %object
	.size	model_default, 4896
model_default:
	.word	16
	.word	51
	.word	924254469
	.word	2019033088
	.word	-1164375549
	.word	-304533996
	.word	-1177005241
	.word	2006476123
	.word	911700331
	.word	521497972
	.word	1614834025
	.word	-1568575639
	.word	-473792395
	.word	-100943719
	.word	-1008475950
	.word	-2086340882
	.word	1113784316
	.word	2226173
	.word	16
	.word	54
	.word	-1564605440
	.word	1887969040
	.word	1113079854
	.word	489581652
	.word	68067460
	.word	167892668
	.word	941488620
	.word	1884618749
	.word	-1467155714
	.word	-624763409
	.word	-1002805
	.word	-487095145
	.word	-1429769094
	.word	1921938546
	.word	995524987
	.word	69244555
	.word	16
	.word	55
	.word	301989889
	.word	925106176
	.word	1531510784
	.word	-2140274688
	.word	-1517092864
	.word	-893976576
	.word	-270793984
	.word	-100851954
	.word	-487703248
	.word	-975940022
	.word	-1481151391
	.word	-1936030343
	.word	1904123795
	.word	1482931117
	.word	1129110473
	.word	1010696172
	.word	16
	.word	56
	.word	-620821748
	.word	-1900806144
	.word	1097272326
	.word	1847574
	.word	1126061128
	.word	-1888918691
	.word	-776631697
	.word	-824192849
	.word	-2001930778
	.word	1013121021
	.word	119066097
	.word	873763754
	.word	2052418941
	.word	-1046589600
	.word	-102425540
	.word	-1243873275
	.word	16
	.word	56
	.word	1652368158
	.word	793327660
	.word	51862088
	.word	554271090
	.word	1446811020
	.word	-2056343390
	.word	-1885742396
	.word	1534521061
	.word	608173557
	.word	151903220
	.word	689482946
	.word	1295680663
	.word	2019778930
	.word	-1550958512
	.word	-793108690
	.word	-1638385
	.word	16
	.word	57
	.word	-385939691
	.word	-1261567999
	.word	-2120612608
	.word	1332221196
	.word	506866206
	.word	807996
	.word	655192425
	.word	1531015795
	.word	-1871288723
	.word	-1113109926
	.word	-573628874
	.word	-925421029
	.word	-1262591157
	.word	-1532193155
	.word	-1784886608
	.word	-1987051548
	.word	16
	.word	65
	.word	-1362493429
	.word	1955597826
	.word	1096297755
	.word	405428546
	.word	34182005
	.word	100716465
	.word	739638506
	.word	1716187646
	.word	-1786853155
	.word	-1247243088
	.word	-910206858
	.word	-674030534
	.word	-724086476
	.word	-741110417
	.word	-606811733
	.word	-1442586
	.word	16
	.word	65
	.word	-1901395710
	.word	1450251015
	.word	742341157
	.word	236677969
	.word	304777
	.word	235068866
	.word	1042480114
	.word	1918626042
	.word	-1836796208
	.word	-1499627621
	.word	-1280424606
	.word	-1212675288
	.word	-1482282740
	.word	-1314233530
	.word	-792816515
	.word	-1590361
	.word	16
	.word	66
	.word	1448679936
	.word	1196314165
	.word	876577128
	.word	606909852
	.word	253487054
	.word	386784244
	.word	1009104068
	.word	1851038886
	.word	-1551329383
	.word	-658661994
	.word	-51265880
	.word	-739646252
	.word	-1598359824
	.word	1803943162
	.word	911278077
	.word	1833983
	.word	16
	.word	67
	.word	-83942601
	.word	-622065389
	.word	-1329266685
	.word	-2019818240
	.word	1601310473
	.word	994846490
	.word	489375028
	.word	135359062
	.word	234367
	.word	168017064
	.word	622190030
	.word	1262023400
	.word	1969291257
	.word	-1635125251
	.word	-961353488
	.word	-287713313
	.word	16
	.word	67
	.word	-1396768512
	.word	-2036790780
	.word	1685396242
	.word	1163080229
	.word	725045569
	.word	388067680
	.word	135239042
	.word	17021864
	.word	33612495
	.word	537917937
	.word	1177812732
	.word	1817837567
	.word	-1820263171
	.word	-1180240647
	.word	-590617363
	.word	-1126694
	.word	16
	.word	67
	.word	1466370560
	.word	977871126
	.word	573390126
	.word	253189709
	.word	67666287
	.word	108179
	.word	50383544
	.word	235400668
	.word	790494971
	.word	1380054524
	.word	1935860460
	.word	-1904162346
	.word	-1466191940
	.word	-1011510111
	.word	-506298743
	.word	-1022349
	.word	16
	.word	68
	.word	-1716157041
	.word	1601996931
	.word	675454095
	.word	17811115
	.word	604828897
	.word	1564606203
	.word	-1787104772
	.word	-1045574688
	.word	-758345041
	.word	-741189515
	.word	-673767621
	.word	-673768702
	.word	-724085448
	.word	-724267150
	.word	-505820756
	.word	-1641242
	.word	16
	.word	69
	.word	958174351
	.word	1935054229
	.word	-1383036270
	.word	-574068866
	.word	-236048307
	.word	-1008923365
	.word	-1985544190
	.word	1399657220
	.word	591084069
	.word	135491667
	.word	50440845
	.word	571399366
	.word	1463482350
	.word	-1854603268
	.word	-894503169
	.word	-1713173
	.word	16
	.word	71
	.word	-1429602304
	.word	1770651648
	.word	709431558
	.word	999974
	.word	587624545
	.word	1682142836
	.word	-1551669132
	.word	-624933031
	.word	-85193942
	.word	-17088481
	.word	-32416
	.word	-167919455
	.word	-907415841
	.word	-1985347591
	.word	1214905087
	.word	153674739
	.word	16
	.word	71
	.word	-352381919
	.word	-1177419000
	.word	-2002780160
	.word	1466961669
	.word	725558808
	.word	152522039
	.word	229222
	.word	218476439
	.word	706403015
	.word	1497427429
	.word	-1955400973
	.word	-1130042898
	.word	-455947298
	.word	-68117325
	.word	-168243531
	.word	-370016282
	.word	16
	.word	72
	.word	50467584
	.word	101005862
	.word	185229389
	.word	185304948
	.word	168537754
	.word	84333761
	.word	195560
	.word	285532400
	.word	773768145
	.word	1346219445
	.word	2003084711
	.word	-1635081822
	.word	-994991200
	.word	-354898520
	.word	-470595
	.word	-1820
	.word	16
	.word	75
	.word	488251392
	.word	387604544
	.word	286564480
	.word	101638080
	.word	285270783
	.word	874555589
	.word	1867484053
	.word	-1349549435
	.word	-271612285
	.word	-503532640
	.word	-1597914437
	.word	1619052229
	.word	742443463
	.word	1816975569
	.word	-1400050977
	.word	-305332493
	.word	16
	.word	75
	.word	573839616
	.word	353915958
	.word	236032114
	.word	202165422
	.word	118093546
	.word	168278488
	.word	689472926
	.word	1530945904
	.word	-1871092136
	.word	1652329340
	.word	709339034
	.word	286111921
	.word	1294974908
	.word	-1989424444
	.word	-1012475439
	.word	-1970718
	.word	16
	.word	76
	.word	1061817344
	.word	892808220
	.word	741428541
	.word	640249437
	.word	455183998
	.word	320253855
	.word	152096704
	.word	324064
	.word	369491967
	.word	925368319
	.word	1481179135
	.word	2053766655
	.word	-1685390083
	.word	-1129579268
	.word	-573703429
	.word	-1115655
	.word	16
	.word	77
	.word	151052543
	.word	538419392
	.word	975790977
	.word	1313088579
	.word	1515728912
	.word	1801416527
	.word	1953607311
	.word	2037903054
	.word	-1937717787
	.word	-1583839579
	.word	-1163049114
	.word	-741930455
	.word	-572902122
	.word	-404654763
	.word	-185551724
	.word	-330540
	.word	16
	.word	77
	.word	234897946
	.word	336892778
	.word	337044411
	.word	403947752
	.word	538668951
	.word	791027271
	.word	1716333577
	.word	2054388820
	.word	2088488357
	.word	2121977077
	.word	-1954251868
	.word	-1668076717
	.word	-709686007
	.word	-320709323
	.word	-84562299
	.word	-131114
	.word	16
	.word	77
	.word	16836351
	.word	100905933
	.word	235503770
	.word	521555559
	.word	858332468
	.word	1564868617
	.word	2037329429
	.word	-2004721594
	.word	-1869918359
	.word	-1567154121
	.word	-894303478
	.word	-337761259
	.word	-101425338
	.word	-17001864
	.word	-14933
	.word	-1826
	.word	16
	.word	77
	.word	318975
	.word	67148219
	.word	369972854
	.word	841029173
	.word	1884160002
	.word	-1601228783
	.word	-1246858416
	.word	-1111836779
	.word	-1044320550
	.word	-1027495476
	.word	-926653049
	.word	-640802750
	.word	-152293877
	.word	-50630064
	.word	-33704043
	.word	-66343
	.word	16
	.word	77
	.word	227471
	.word	184764768
	.word	622206770
	.word	1329006607
	.word	1818373142
	.word	2004048964
	.word	2088339060
	.word	-1937549202
	.word	-1617614015
	.word	-1112865258
	.word	-422244096
	.word	-135184601
	.word	-50696361
	.word	-155769
	.word	-12361
	.word	2097127
	.word	16
	.word	333
	.word	-234888961
	.word	-588733760
	.word	-925801854
	.word	-1144970429
	.word	-1431164405
	.word	-1616614838
	.word	-1835488887
	.word	-2138380087
	.word	1869922270
	.word	1634303903
	.word	1432043616
	.word	1179582752
	.word	909983519
	.word	674201182
	.word	354401438
	.word	720092
	.word	16
	.word	78
	.word	67168255
	.word	319535311
	.word	673089953
	.word	942692977
	.word	1363487811
	.word	1734155539
	.word	1751666230
	.word	1802076006
	.word	1919922327
	.word	2020991176
	.word	-1870731016
	.word	-1482769454
	.word	-1078751324
	.word	-775331977
	.word	-421908152
	.word	-917479
	.word	12
	.word	79
	.word	775893279
	.word	270425158
	.word	50892662
	.word	115368
	.word	503900122
	.word	1328938484
	.word	-2123825921
	.word	-1281626379
	.word	-624439585
	.word	-202792776
	.word	-17207929
	.word	-100713388
	.word	-622062810
	.word	-1430060792
	.word	2005926144
	.word	1180570631
	.word	12
	.word	79
	.word	-2039479797
	.word	-1197468921
	.word	-473025501
	.word	-118462900
	.word	-222335
	.word	-235221323
	.word	-823855899
	.word	-1699414021
	.word	1702887423
	.word	911010552
	.word	371441879
	.word	626343
	.word	100751731
	.word	605171265
	.word	1295518748
	.word	-2107178748
	.word	12
	.word	79
	.word	-790429165
	.word	-1665794048
	.word	1753353221
	.word	944710678
	.word	388320310
	.word	67928166
	.word	111770
	.word	319153358
	.word	1143732976
	.word	2019491838
	.word	-1416433159
	.word	-742273053
	.word	-236740167
	.word	-364920
	.word	-117490860
	.word	-706211804
	.word	12
	.word	79
	.word	1013386
	.word	151012701
	.word	722935344
	.word	1514276369
	.word	-1921777408
	.word	-1130031349
	.word	-472762332
	.word	-51223476
	.word	-83912834
	.word	-470760273
	.word	-1110251817
	.word	-1901593870
	.word	1534394110
	.word	776268278
	.word	169591510
	.word	33722281
	.word	12
	.word	79
	.word	68061809
	.word	184794276
	.word	739895252
	.word	1564737269
	.word	-1871185410
	.word	-1096229138
	.word	-540035381
	.word	-135428705
	.word	-16952981
	.word	-420077001
	.word	-1211103214
	.word	-2086862846
	.word	1332283137
	.word	540483345
	.word	68046388
	.word	164456
	.word	12
	.word	79
	.word	-2103703296
	.word	1315447573
	.word	607668788
	.word	135625825
	.word	177560
	.word	218361297
	.word	1126629373
	.word	2053107448
	.word	-1416376352
	.word	-708728899
	.word	-186222960
	.word	-67158694
	.word	-806610138
	.word	-1766647784
	.word	1585063197
	.word	675494703
	.word	12
	.word	79
	.word	941514344
	.word	1850954070
	.word	-1551347633
	.word	-658612650
	.word	-101873811
	.word	-100877663
	.word	-655563310
	.word	-1514079758
	.word	1888223229
	.word	995490814
	.word	337893357
	.word	435391
	.word	151154825
	.word	638927191
	.word	1345920558
	.word	2120679440
	.word	16
	.word	81
	.word	-907804145
	.word	-1918173184
	.word	1366231042
	.word	406069009
	.word	33969463
	.word	890793832
	.word	1901292399
	.word	-1433511581
	.word	-557499829
	.word	-17561308
	.word	-504414684
	.word	-1060081321
	.word	-1414222706
	.word	-1650136886
	.word	-978653187
	.word	-35596574
	.word	16
	.word	83
	.word	-1211170815
	.word	-2036464895
	.word	1433209093
	.word	658382610
	.word	51463212
	.word	537486938
	.word	1362653803
	.word	-2106951562
	.word	-1281654661
	.word	-540305013
	.word	-672089
	.word	-588063024
	.word	-1379470872
	.word	2090138615
	.word	1264844797
	.word	422772735
	.word	16
	.word	83
	.word	-874642944
	.word	-1464200695
	.word	-2053692390
	.word	1786068783
	.word	-2140383407
	.word	-1500291493
	.word	-860264862
	.word	-220171157
	.word	-50359166
	.word	-521096538
	.word	-1110457669
	.word	-1750347319
	.word	1938152919
	.word	1281419491
	.word	658175214
	.word	1376249
	.word	16
	.word	85
	.word	84746761
	.word	87868
	.word	50367601
	.word	285654950
	.word	790359002
	.word	1615520240
	.word	-2005680947
	.word	-1516662616
	.word	-1129291912
	.word	-842585018
	.word	-791672559
	.word	-909427164
	.word	-959941799
	.word	-909727603
	.word	-657269310
	.word	-1769482
	.word	16
	.word	86
	.word	150998784
	.word	420556831
	.word	656429632
	.word	875394403
	.word	1094358917
	.word	1330100648
	.word	1616370121
	.word	1969879530
	.word	-1803094785
	.word	-1449142811
	.word	-1196378939
	.word	-910126173
	.word	-607030140
	.word	-370978205
	.word	-168874175
	.word	-455907
	.word	16
	.word	86
	.word	117473908
	.word	386899344
	.word	656389802
	.word	942657730
	.word	1178594269
	.word	1397618682
	.word	1666964457
	.word	1902821838
	.word	-2105890892
	.word	-1836413286
	.word	-1550158206
	.word	-1230217622
	.word	-926988461
	.word	-623824837
	.word	-303818460
	.word	-786419
	.word	16
	.word	86
	.word	117641984
	.word	151533862
	.word	151609164
	.word	134776178
	.word	134785944
	.word	101241534
	.word	17103077
	.word	369682935
	.word	858116060
	.word	1329706434
	.word	1834916776
	.word	-1988394610
	.word	-1499961227
	.word	-994750629
	.word	-506317245
	.word	-1107159
	.word	24
	.word	86
	.word	134221568
	.word	252456735
	.word	303190080
	.word	319975520
	.word	370446721
	.word	437825953
	.word	471585474
	.word	538833634
	.word	858060798
	.word	1329719279
	.word	1834932447
	.word	-1971599152
	.word	-1499940928
	.word	-1011571025
	.word	-523070051
	.word	-1145715
	.word	16
	.word	87
	.word	100671232
	.word	286023487
	.word	471375743
	.word	673373886
	.word	1093788669
	.word	1431020739
	.word	1700553860
	.word	2020549700
	.word	-2072039665
	.word	-1970901169
	.word	-1836142961
	.word	-1634079026
	.word	-1163079442
	.word	-708275277
	.word	-337620109
	.word	-650445
	.word	16
	.word	87
	.word	83923324
	.word	369998758
	.word	656401360
	.word	1077080825
	.word	1414253535
	.word	1633329333
	.word	1835564426
	.word	2088019603
	.word	-1954164292
	.word	-1651378201
	.word	-1280716316
	.word	-994268996
	.word	-758415469
	.word	-489007510
	.word	-219468735
	.word	-524266
	.word	16
	.word	87
	.word	11531
	.word	94543
	.word	151172499
	.word	672592852
	.word	1480905969
	.word	2036963254
	.word	-1971039371
	.word	-2054549711
	.word	2139114024
	.word	2122223212
	.word	-1971137872
	.word	-1248396044
	.word	-456078617
	.word	-51082837
	.word	-113305
	.word	-100859869
	.word	24
	.word	87
	.word	402960
	.word	50420801
	.word	100962935
	.word	252299437
	.word	689502176
	.word	1363338491
	.word	1751036619
	.word	2003860631
	.word	-2055368329
	.word	-1752316501
	.word	-1264454690
	.word	-607398420
	.word	-337337667
	.word	-185570169
	.word	-84461999
	.word	-196581
	.word	16
	.word	87
	.word	201335819
	.word	538269245
	.word	858491248
	.word	1262578564
	.word	1532180561
	.word	1784874527
	.word	2020748564
	.word	-2122555578
	.word	-1954114951
	.word	-1752054613
	.word	-1549862946
	.word	-1230055954
	.word	-960585029
	.word	-657493879
	.word	-303874730
	.word	-652764
	.word	16
	.word	87
	.word	218116633
	.word	504718667
	.word	740595838
	.word	926075825
	.word	1161690852
	.word	1447939559
	.word	1734187956
	.word	2070964354
	.word	-1870383793
	.word	-1550122468
	.word	-1381486313
	.word	-1162714038
	.word	-926902403
	.word	-556501103
	.word	-236501922
	.word	-585429
	.word	16
	.word	87
	.word	201336328
	.word	605578052
	.word	925803652
	.word	1262544068
	.word	1750588409
	.word	-2123063620
	.word	-1550687617
	.word	-1028380602
	.word	-741533433
	.word	-707438279
	.word	-740976006
	.word	-774645062
	.word	-589967891
	.word	-354120275
	.word	-152023956
	.word	-193748
	.word	16
	.word	343
	.word	-134275072
	.word	-454205376
	.word	-707026559
	.word	-1110712128
	.word	-1599417357
	.word	-1869049165
	.word	-2071309710
	.word	-2139025103
	.word	2004696349
	.word	1668120158
	.word	1398521758
	.word	1011416542
	.word	657441250
	.word	303727266
	.word	51003745
	.word	131105
	.word	16
	.word	89
	.word	369101587
	.word	1143800579
	.word	1263023131
	.word	1632656971
	.word	-1854317470
	.word	-1129888431
	.word	-506452425
	.word	-51114988
	.word	-386844382
	.word	-605984431
	.word	-690382206
	.word	-824980558
	.word	-1329270559
	.word	2140732407
	.word	1315438333
	.word	490143231
	.word	16
	.word	90
	.word	806748672
	.word	1682572291
	.word	-1719729405
	.word	-843906558
	.word	-404222720
	.word	-992594398
	.word	-1682940854
	.word	1938128748
	.word	1348577427
	.word	691981499
	.word	1307357
	.word	756284669
	.word	1648948984
	.word	-1770196493
	.word	-877531665
	.word	-1707540
	.word	16
	.word	60
	.word	-251720960
	.word	-773776364
	.word	-1329583068
	.word	-1868481485
	.word	1904233540
	.word	1365269588
	.word	792751460
	.word	220101749
	.word	369462662
	.word	942119828
	.word	1464314274
	.word	2003351985
	.word	-1769551424
	.word	-1247356207
	.word	-708383517
	.word	-169541642
	.word	16
	.word	62
	.word	386336256
	.word	958928399
	.word	1497966367
	.word	2037004079
	.word	-1702213569
	.word	-1180018351
	.word	-657953950
	.word	-85361036
	.word	-470512251
	.word	-1043161963
	.word	-1615811420
	.word	2123283123
	.word	1567476674
	.word	1011669971
	.word	523103460
	.word	1114101
	.word	16
	.word	4097
	.word	257279
	.word	84068057
	.word	185180082
	.word	336623756
	.word	572215909
	.word	908799808
	.word	1396773405
	.word	2036728330
	.word	-1601371904
	.word	-1011673333
	.word	-640796120
	.word	-303931571
	.word	-185432462
	.word	-84497255
	.word	-142400
	.word	-1306
	.word	16
	.word	4098
	.word	419464317
	.word	1278379909
	.word	2137361288
	.word	-1315403381
	.word	-473202808
	.word	-118337688
	.word	-319283915
	.word	-1076297461
	.word	-1868163836
	.word	-2121906643
	.word	2088597856
	.word	2138811539
	.word	-2004623418
	.word	-1348993036
	.word	-590877958
	.word	-1130025
	.word	16
	.word	4099
	.word	1967330330
	.word	-676844009
	.word	-117993636
	.word	-957945409
	.word	1687681274
	.word	322483163
	.word	50483074
	.word	1243350560
	.word	-1350823935
	.word	-119908309
	.word	-352404341
	.word	-1697907738
	.word	946401023
	.word	169382596
	.word	655503455
	.word	-2058220015
	.word	16
	.word	4100
	.word	67121668
	.word	302747232
	.word	387247037
	.word	791132390
	.word	1127766921
	.word	1531708205
	.word	1785094194
	.word	2003877006
	.word	-1937590042
	.word	-1684775798
	.word	-1365114579
	.word	-1145610961
	.word	-910050676
	.word	-472531737
	.word	-236298355
	.word	-720335
	.word	16
	.word	4101
	.word	419430922
	.word	1295188992
	.word	2087000074
	.word	-1584447701
	.word	-1280544168
	.word	-1128749685
	.word	-1329997889
	.word	-1969160980
	.word	1500639997
	.word	1095484123
	.word	1128238760
	.word	1397382518
	.word	1902195270
	.word	-1652223970
	.word	-793377271
	.word	-1436663
	.word	16
	.word	4102
	.word	-553711852
	.word	-1698955262
	.word	1467550210
	.word	490156056
	.word	17263947
	.word	789551242
	.word	1917951132
	.word	-1298887780
	.word	-1547937941
	.word	1619088979
	.word	641825123
	.word	34517393
	.word	537257939
	.word	1682111476
	.word	-1484391425
	.word	-339089421
	.word	16
	.word	4103
	.word	-151058944
	.word	-454224884
	.word	-706927075
	.word	-976406994
	.word	-1279506882
	.word	-1565894834
	.word	-1818597280
	.word	-2088142480
	.word	1954318978
	.word	1684839571
	.word	1381739684
	.word	1128971700
	.word	859557830
	.word	573234645
	.word	303820518
	.word	655350
	.word	16
	.word	4104
	.word	150997248
	.word	420551443
	.word	673197348
	.word	959462709
	.word	1229082181
	.word	1515281750
	.word	1801612645
	.word	2104785780
	.word	-1903785086
	.word	-1651073901
	.word	-1398493531
	.word	-1129005130
	.word	-893070905
	.word	-606936615
	.word	-320605719
	.word	-655368
	.word	16
	.word	4105
	.word	167836415
	.word	487910900
	.word	774233317
	.word	1043778516
	.word	1313127106
	.word	1599580850
	.word	1869060770
	.word	2138540176
	.word	-1937410690
	.word	-1667996821
	.word	-1415360166
	.word	-1145945782
	.word	-876400583
	.word	-573366488
	.word	-286977767
	.word	-589814
	.word	16
	.word	4106
	.word	-184550657
	.word	-504696842
	.word	-774251034
	.word	-1043673643
	.word	-1262765373
	.word	-1565742928
	.word	-1835361887
	.word	-2088204399
	.word	1987933309
	.word	1718510443
	.word	1398559838
	.word	1145913931
	.word	859648570
	.word	606806314
	.word	337383192
	.word	655368
	.word	16
	.word	4107
	.word	805306888
	.word	-1872756224
	.word	-239002100
	.word	-1243198145
	.word	1434801747
	.word	841241942
	.word	-1822271658
	.word	-238848930
	.word	-1159026281
	.word	1502261936
	.word	254388913
	.word	1883285671
	.word	-777996114
	.word	-788992816
	.word	1872755957
	.word	239074559
	.word	16
	.word	4108
	.word	637539340
	.word	1716077094
	.word	-2105766554
	.word	1837160628
	.word	676330228
	.word	168475104
	.word	1092583575
	.word	-1905749630
	.word	-692813932
	.word	-101566112
	.word	-705167342
	.word	-1850792182
	.word	2122543948
	.word	2122170778
	.word	-1265567003
	.word	-2364930
	.word	16
	.word	4109
	.word	1989020672
	.word	1180575257
	.word	422528565
	.word	17265754
	.word	840403589
	.word	1733133706
	.word	-1686014851
	.word	-860595095
	.word	-136103091
	.word	-335737567
	.word	-1227882492
	.word	-1818351859
	.word	-2004001470
	.word	-2088397960
	.word	2088815022
	.word	1819672548
	.word	16
	.word	4111
	.word	234943743
	.word	790553836
	.word	1329517788
	.word	1868547276
	.word	-1887456324
	.word	-1365335125
	.word	-826370917
	.word	-253786739
	.word	-251694982
	.word	-807443864
	.word	-1380036006
	.word	-1919007926
	.word	1836922172
	.word	1281107502
	.word	742135070
	.word	205258763
	.text
	.align	2
	.type	SortBubble, %function
SortBubble:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #28
	str	r0, [fp, #-24]
	str	r1, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L2
.L6:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-8]
	b	.L3
.L5:
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bge	.L4
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #2
	ldr	r1, [fp, #-24]
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r2, [fp, #-16]
	str	r2, [r3, #0]
.L4:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L3:
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	blt	.L5
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L2:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	blt	.L6
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	SortBubble, .-SortBubble
	.align	2
	.type	Sqrt, %function
Sqrt:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-12]
	mov	r3, #14
	str	r3, [fp, #-8]
	b	.L9
.L11:
	mov	r2, #1
	ldr	r3, [fp, #-8]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-12]
	add	r3, r2, r3
	mov	r1, #1
	ldr	r2, [fp, #-8]
	mov	r1, r1, asl r2
	ldr	r2, [fp, #-12]
	add	r1, r1, r2
	mul	r2, r1, r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bgt	.L10
	mov	r2, #1
	ldr	r3, [fp, #-8]
	mov	r3, r2, asl r3
	ldr	r2, [fp, #-12]
	orr	r3, r2, r3
	str	r3, [fp, #-12]
.L10:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L9:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bge	.L11
	ldr	r3, [fp, #-12]
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	Sqrt, .-Sqrt
	.global	__aeabi_idiv
	.align	2
	.type	PointRange, %function
PointRange:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	str	r2, [fp, #-16]
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bgt	.L14
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	beq	.L15
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	ldr	r2, [fp, #-16]
	rsb	r1, r2, #1
	mul	r2, r1, r3
	ldr	r1, [fp, #-8]
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	str	r3, [fp, #-12]
.L15:
	mov	r3, #1
	str	r3, [fp, #-8]
.L14:
	ldr	r3, .L27
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bgt	.L16
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	beq	.L17
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	ldr	r2, .L27
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	ldr	r2, [fp, #-16]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r1, [fp, #-8]
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	str	r3, [fp, #-12]
.L17:
	ldr	r3, .L27
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L16:
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bgt	.L18
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	beq	.L19
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r2
	ldr	r2, [fp, #-20]
	rsb	r1, r2, #1
	mul	r2, r1, r3
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L19:
	mov	r3, #1
	str	r3, [fp, #-12]
.L18:
	ldr	r3, .L27+4
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bgt	.L20
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	beq	.L21
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r2
	ldr	r2, .L27+4
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	ldr	r2, [fp, #-20]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L21:
	ldr	r3, .L27+4
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-12]
.L20:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bgt	.L22
	mov	r3, #1
	str	r3, [fp, #-8]
.L22:
	ldr	r3, .L27
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bgt	.L23
	ldr	r3, .L27
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L23:
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bgt	.L24
	mov	r3, #1
	str	r3, [fp, #-12]
.L24:
	ldr	r3, .L27+4
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bgt	.L25
	ldr	r3, .L27+4
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-12]
.L25:
	ldr	r3, [fp, #-8]
	mov	r2, r3, asl #16
	ldr	r3, [fp, #-12]
	add	r3, r2, r3
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L28:
	.align	2
.L27:
	.word	drv_num_nokey
	.word	sen_num_nokey
	.size	PointRange, .-PointRange
	.align	2
	.type	PointCoor, %function
PointCoor:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L30
.L33:
	ldr	r3, .L35
	ldrb	r3, [r3, #2]	@ zero_extendqisi2
	and	r3, r3, #128
	cmp	r3, #0
	beq	.L31
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	ldr	r3, .L35+4
	ldr	r3, [r3, r1, asl #2]
	bic	r1, r3, #40960
	ldr	r3, .L35+4
	str	r1, [r3, r2, asl #2]
	b	.L32
.L31:
	ldr	r1, [fp, #-8]
	ldr	r2, [fp, #-8]
	ldr	r3, .L35+4
	ldr	r3, [r3, r2, asl #2]
	bic	r3, r3, #-268435456
	bic	r3, r3, #40960
	ldr	r2, .L35+4
	str	r3, [r2, r1, asl #2]
.L32:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L30:
	ldr	r3, .L35+8
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-8]
	cmp	r2, r3
	blt	.L33
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L36:
	.align	2
.L35:
	.word	global_state
	.word	point_now
	.word	point_num
	.size	PointCoor, .-PointCoor
	.align	2
	.type	PointRepeat, %function
PointRepeat:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #44
	ldr	r3, .L61
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L38
	ldr	r3, .L61
	ldr	r3, [r3, #0]
	sub	r2, r3, #1
	ldr	r3, .L61
	str	r2, [r3, #0]
.L38:
	ldr	r3, .L61+4
	ldr	r2, [r3, #0]
	ldr	r3, .L61+8
	ldr	r3, [r3, #0]
	cmp	r2, r3
	ble	.L39
	ldr	r3, .L61
	mov	r2, #8
	str	r2, [r3, #0]
.L39:
	ldr	r3, .L61+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L40
	ldr	r3, .L61+12
	ldr	r3, [r3, #4]
	cmp	r3, #0
	bne	.L41
.L40:
	ldr	r3, .L61
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L42
	mov	r3, #96
	str	r3, [fp, #-8]
	b	.L44
.L42:
	mov	r3, #32
	str	r3, [fp, #-8]
	b	.L44
.L41:
	ldr	r3, .L61
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L45
	ldr	r3, .L61+12
	ldr	r3, [r3, #4]
	str	r3, [fp, #-8]
	b	.L44
.L45:
	ldr	r3, .L61+12
	ldr	r3, [r3, #0]
	str	r3, [fp, #-8]
.L44:
	mov	r3, #0
	str	r3, [fp, #-40]
	b	.L46
.L55:
	ldr	r2, [fp, #-40]
	ldr	r3, .L61+16
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L57
.L47:
	ldr	r2, [fp, #-40]
	ldr	r1, .L61+16
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L58
.L49:
	ldr	r2, [fp, #-40]
	ldr	r1, .L61+16
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r2
	str	r3, [fp, #-24]
	ldr	r2, [fp, #-40]
	ldr	r1, .L61+16
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-40]
	ldr	r2, .L61+16
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r2
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-40]
	ldr	r2, .L61+16
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-40]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	b	.L50
.L54:
	ldr	r2, [fp, #-36]
	ldr	r3, .L61+16
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L59
.L51:
	ldr	r2, [fp, #-36]
	ldr	r1, .L61+16
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L60
.L53:
	ldr	r2, [fp, #-36]
	ldr	r1, .L61+16
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-36]
	ldr	r2, .L61+16
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-28]
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	ble	.L52
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bge	.L52
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	ble	.L52
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L52
	ldr	r1, [fp, #-40]
	ldr	r2, [fp, #-40]
	ldr	r0, .L61+16
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r0, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r0, [fp, #-36]
	ldr	ip, .L61+16
	mov	r3, #2
	mov	r0, r0, asl #2
	add	r0, ip, r0
	add	r3, r0, r3
	ldrh	r3, [r3, #0]
	add	r3, r2, r3
	add	r3, r3, #1
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L61+16
	mov	r3, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	ldr	r0, [fp, #-40]
	ldr	r3, [fp, #-40]
	ldr	r2, .L61+16
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-36]
	ldr	r1, .L61+16
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
	add	r3, r3, #1
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L61+16
	mov	r3, r0, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	ldr	r2, [fp, #-36]
	ldr	r3, .L61+16
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-40]
	sub	r3, r3, #1
	str	r3, [fp, #-40]
	ldr	r3, .L61
	mov	r2, #8
	str	r2, [r3, #0]
	b	.L48
.L59:
	mov	r0, r0	@ nop
	b	.L52
.L60:
	mov	r0, r0	@ nop
.L52:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L50:
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	ble	.L54
	b	.L48
.L57:
	mov	r0, r0	@ nop
	b	.L48
.L58:
	mov	r0, r0	@ nop
.L48:
	ldr	r3, [fp, #-40]
	add	r3, r3, #1
	str	r3, [fp, #-40]
.L46:
	ldr	r3, [fp, #-40]
	cmp	r3, #9
	ble	.L55
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L62:
	.align	2
.L61:
	.word	point_near
	.word	prev_num
	.word	point_num
	.word	point_repeat
	.word	point_now
	.size	PointRepeat, .-PointRepeat
	.align	2
	.type	PointPointer, %function
PointPointer:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	ldr	r3, .L87
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	ldr	r3, .L87
	str	r2, [r3, #0]
	ldr	r3, .L87
	ldr	r2, [r3, #0]
	ldr	r3, .L87+4
	cmp	r2, r3
	ble	.L64
	ldr	r3, .L87
	mov	r2, #0
	str	r2, [r3, #0]
.L64:
	ldr	r3, .L87
	ldr	r2, [r3, #0]
	ldr	r1, .L87+8
	smull	r0, r3, r1, r2
	mov	r1, r3, asr #2
	mov	r3, r2, asr #31
	rsb	r1, r3, r1
	mov	r3, r1
	mov	r3, r3, asl #2
	add	r3, r3, r1
	mov	r3, r3, asl #1
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L65
.L68:
	ldr	r1, [fp, #-12]
	ldr	r0, .L87+12
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	add	r2, r0, r3
	ldr	r3, .L87+16
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bne	.L66
	mov	r3, #9
	str	r3, [fp, #-8]
	b	.L67
.L66:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L67:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L65:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L68
	ldr	r3, .L87
	ldr	r2, [r3, #0]
	ldr	r1, .L87+8
	smull	r0, r3, r1, r2
	mov	r1, r3, asr #2
	mov	r3, r2, asr #31
	rsb	r1, r3, r1
	mov	r3, r1
	mov	r3, r3, asl #2
	add	r3, r3, r1
	mov	r3, r3, asl #1
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L69
.L72:
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	add	r2, r3, #400
	ldr	r3, .L87+12
	add	r3, r2, r3
	mov	r2, r3
	ldr	r3, .L87+20
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bne	.L70
	mov	r3, #9
	str	r3, [fp, #-8]
	b	.L71
.L70:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L71:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L69:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L72
	ldr	r3, .L87
	ldr	r2, [r3, #0]
	ldr	r1, .L87+8
	smull	r0, r3, r1, r2
	mov	r1, r3, asr #2
	mov	r3, r2, asr #31
	rsb	r1, r3, r1
	mov	r3, r1
	mov	r3, r3, asl #2
	add	r3, r3, r1
	mov	r3, r3, asl #1
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L73
.L76:
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	add	r2, r3, #800
	ldr	r3, .L87+12
	add	r3, r2, r3
	mov	r2, r3
	ldr	r3, .L87+24
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bne	.L74
	mov	r3, #9
	str	r3, [fp, #-8]
	b	.L75
.L74:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L75:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L73:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L76
	ldr	r3, .L87
	ldr	r2, [r3, #0]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #29
	add	r2, r2, r3
	and	r2, r2, #7
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L77
.L80:
	ldr	r1, [fp, #-12]
	ldr	r0, .L87+28
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	add	r2, r0, r3
	ldr	r3, .L87+32
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bne	.L78
	mov	r3, #7
	str	r3, [fp, #-8]
	b	.L79
.L78:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L79:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L77:
	ldr	r3, [fp, #-12]
	cmp	r3, #7
	ble	.L80
	mov	r3, #0
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L81
.L83:
	ldr	r2, [fp, #-12]
	ldr	r3, .L87+36
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L82
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-12]
	ldr	r3, .L87+36
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, .L87+36
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L82:
	ldr	r3, .L87+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L87+20
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L87+24
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L81:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L83
	ldr	r3, .L87+40
	ldr	r2, [fp, #-8]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	str	r3, [fp, #-12]
	b	.L84
.L85:
	ldr	r2, [fp, #-12]
	ldr	r3, .L87+36
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L84:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L85
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L88:
	.align	2
.L87:
	.word	point_n
	.word	7999
	.word	1717986919
	.word	point_array
	.word	point_pointer
	.word	point_stretch
	.word	point_report
	.word	pressure_array
	.word	pressure_pointer
	.word	point_now
	.word	point_num
	.size	PointPointer, .-PointPointer
	.align	2
	.type	CCO, %function
CCO:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	str	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L90
	ldr	r3, [fp, #-8]
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L91
	ldr	r3, [fp, #-8]
	bic	r2, r3, #31
	ldr	r3, [fp, #-8]
	mvn	r3, r3
	and	r3, r3, #31
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-12]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3
	and	r3, r3, #31
	add	r3, r2, r3
	b	.L92
.L91:
	ldr	r3, [fp, #-8]
	bic	r2, r3, #31
	ldr	r3, [fp, #-8]
	and	r3, r3, #31
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-12]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #31
	add	r3, r2, r3
	b	.L92
.L90:
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bne	.L93
	ldr	r3, [fp, #-8]
	and	r3, r3, #64
	cmp	r3, #0
	beq	.L94
	ldr	r3, [fp, #-8]
	bic	r2, r3, #63
	ldr	r3, [fp, #-8]
	mvn	r3, r3
	and	r3, r3, #63
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-12]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3
	and	r3, r3, #63
	add	r3, r2, r3
	b	.L92
.L94:
	ldr	r3, [fp, #-8]
	bic	r2, r3, #63
	ldr	r3, [fp, #-8]
	and	r3, r3, #63
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-12]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #63
	add	r3, r2, r3
	b	.L92
.L93:
	ldr	r3, [fp, #-16]
	cmp	r3, #2
	bne	.L95
	ldr	r3, [fp, #-8]
	bic	r2, r3, #63
	ldr	r3, [fp, #-8]
	and	r3, r3, #63
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-12]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #63
	add	r3, r2, r3
	b	.L92
.L95:
	mov	r3, #0
.L92:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	CCO, .-CCO
	.align	2
	.type	CoordinateCorrect, %function
CoordinateCorrect:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 128
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #132
	mov	r3, #64
	str	r3, [fp, #-24]
	ldr	r3, .L140
	ldr	r3, [r3, #0]
	and	r3, r3, #15
	cmp	r3, #0
	beq	.L136
.L98:
	ldr	r3, .L140
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #4
	and	r3, r3, #15
	str	r3, [fp, #-20]
	ldr	r3, .L140
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	and	r3, r3, #15
	str	r3, [fp, #-16]
	ldr	r3, .L140+4
	str	r3, [fp, #-52]
	ldr	r3, .L140+8
	str	r3, [fp, #-72]
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L100
.L101:
	ldr	r3, [fp, #-32]
	add	r2, r3, #1
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r2, r3, #1
	mvn	r3, #59
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L100:
	ldr	r3, [fp, #-32]
	cmp	r3, #3
	ble	.L101
	ldr	r3, [fp, #-20]
	cmp	r3, #3
	beq	.L102
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	bne	.L103
.L102:
	mov	r3, #0
	str	r3, [fp, #-32]
	ldr	r3, .L140
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #4
	and	r3, r3, #15
	cmp	r3, #3
	bne	.L104
	ldr	r2, .L140+12
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #8
	add	r3, r2, r3
	str	r3, [fp, #-48]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L104:
	ldr	r3, .L140
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	and	r3, r3, #15
	cmp	r3, #3
	bne	.L137
	ldr	r2, .L140+12
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #8
	add	r3, r2, r3
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
	b	.L106
.L103:
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L107
.L108:
	ldr	r1, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r3, .L140+16
	ldr	r3, [r3, r2, asl #2]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mvn	r2, #91
	mov	r1, r1, asl #3
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r2, r1, r2
	str	r3, [r2, #0]
	ldr	r1, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r3, .L140+16
	ldr	r3, [r3, r2, asl #2]
	mov	r2, r3, lsr #16
	mvn	r3, #87
	mov	r1, r1, asl #3
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r1, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r3, .L140+20
	ldr	r3, [r3, r2, asl #2]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mvn	r2, #123
	mov	r1, r1, asl #3
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r2, r1, r2
	str	r3, [r2, #0]
	ldr	r1, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r3, .L140+20
	ldr	r3, [r3, r2, asl #2]
	mov	r2, r3, lsr #16
	mvn	r3, #119
	mov	r1, r1, asl #3
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L107:
	ldr	r3, [fp, #-32]
	cmp	r3, #3
	ble	.L108
	mov	r3, #1
	str	r3, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L109
.L111:
	ldr	r2, [fp, #-32]
	mvn	r3, #91
	mov	r2, r2, asl #3
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L110
	ldr	r2, [fp, #-32]
	mvn	r3, #87
	mov	r2, r2, asl #3
	sub	ip, fp, #12
	add	r2, ip, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #3
	bhi	.L110
	ldr	r1, [fp, #-28]
	ldr	r2, .L140+12
	ldr	r0, [fp, #-32]
	mvn	r3, #87
	mov	r0, r0, asl #3
	sub	ip, fp, #12
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #8
	add	r2, r2, r3
	mvn	r3, #39
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L110:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L109:
	ldr	r3, [fp, #-32]
	cmp	r3, #3
	ble	.L111
	mov	r3, #1
	str	r3, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L112
.L114:
	ldr	r2, [fp, #-32]
	mvn	r3, #123
	mov	r2, r2, asl #3
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L113
	ldr	r2, [fp, #-32]
	mvn	r3, #119
	mov	r2, r2, asl #3
	sub	ip, fp, #12
	add	r2, ip, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #3
	bhi	.L113
	ldr	r1, [fp, #-28]
	ldr	r2, .L140+12
	ldr	r0, [fp, #-32]
	mvn	r3, #119
	mov	r0, r0, asl #3
	sub	ip, fp, #12
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #8
	add	r2, r2, r3
	mvn	r3, #59
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L113:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L112:
	ldr	r3, [fp, #-32]
	cmp	r3, #3
	ble	.L114
	b	.L106
.L137:
	mov	r0, r0	@ nop
.L106:
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L115
.L134:
	ldr	r2, [fp, #-32]
	ldr	r3, .L140+24
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L138
.L116:
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L139
.L117:
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bcc	.L119
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L140+28
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r1
	cmp	r2, r3
	bhi	.L119
	ldr	r3, .L140+32
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L120
	ldr	r4, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L140+36
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L140+24
	mov	r3, #2
	mov	r1, r4, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	b	.L119
.L120:
	ldr	r3, [fp, #-20]
	cmp	r3, #3
	bne	.L121
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	and	r3, r3, #64
	cmp	r3, #0
	beq	.L122
	ldr	r4, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-52]
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L140+24
	mov	r3, #2
	mov	r1, r4, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	b	.L119
.L122:
	ldr	r4, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-48]
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L140+24
	mov	r3, #2
	mov	r1, r4, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	b	.L119
.L121:
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L124
.L126:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	bgt	.L125
	ldr	r3, [fp, #-28]
	add	r2, r3, #1
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L125
	ldr	r2, [fp, #-28]
	mvn	r3, #91
	mov	r2, r2, asl #3
	sub	ip, fp, #12
	add	r2, ip, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L125
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r1, [fp, #-28]
	mvn	r3, #91
	mov	r1, r1, asl #3
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcc	.L125
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L124
.L125:
	ldr	r4, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r1, .L140+24
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r2, r3
	ldr	r1, [fp, #-28]
	mvn	r3, #39
	mov	r1, r1, asl #2
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	ldr	r2, [fp, #-20]
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L140+24
	mov	r3, #2
	mov	r1, r4, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	b	.L119
.L124:
	ldr	r3, [fp, #-28]
	cmp	r3, #4
	ble	.L126
.L119:
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bcc	.L118
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L140+40
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r1
	cmp	r2, r3
	bhi	.L118
	ldr	r3, .L140+32
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L127
	ldr	r4, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L140+44
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L140+24
	mov	r3, r4, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	b	.L118
.L127:
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	bne	.L128
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	and	r3, r3, #64
	cmp	r3, #0
	beq	.L129
	ldr	r4, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-72]
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L140+24
	mov	r3, r4, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	b	.L118
.L129:
	ldr	r4, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-68]
	mov	r0, r2
	mov	r1, r3
	mov	r2, #2
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L140+24
	mov	r3, r4, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	b	.L118
.L128:
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L131
.L133:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	bgt	.L132
	ldr	r3, [fp, #-28]
	add	r2, r3, #1
	mvn	r3, #59
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L132
	ldr	r2, [fp, #-28]
	mvn	r3, #123
	mov	r2, r2, asl #3
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L132
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, [fp, #-28]
	mvn	r3, #123
	mov	r1, r1, asl #3
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcc	.L132
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L131
.L132:
	ldr	r4, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r2, .L140+24
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, [fp, #-28]
	mvn	r3, #59
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	ldr	r2, [fp, #-16]
	bl	CCO
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L140+24
	mov	r3, r4, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	b	.L118
.L131:
	ldr	r3, [fp, #-28]
	cmp	r3, #4
	ble	.L133
	b	.L118
.L139:
	mov	r0, r0	@ nop
.L118:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L115:
	ldr	r3, .L140+48
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-32]
	cmp	r2, r3
	bge	.L135
	ldr	r3, [fp, #-32]
	cmp	r3, #9
	ble	.L134
	b	.L135
.L136:
	mov	r0, r0	@ nop
	b	.L135
.L138:
	mov	r0, r0	@ nop
.L135:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L141:
	.align	2
.L140:
	.word	coordinate_correct_able
	.word	coordinate_correct_coe_x
	.word	coordinate_correct_coe_y
	.word	multi_group
	.word	multi_x_array
	.word	multi_y_array
	.word	point_now
	.word	drv_num_nokey
	.word	global_state
	.word	multi_group+512
	.word	sen_num_nokey
	.word	multi_group+768
	.word	point_num
	.size	CoordinateCorrect, .-CoordinateCorrect
	.align	2
	.type	PointPredictOne, %function
PointPredictOne:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	ldr	r3, .L144
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L144
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	bic	r3, r3, #-268435456
	bic	r3, r3, #61440
	str	r3, [r2, #0]
	ldr	r3, .L144
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [r2, #0]
	bic	r3, r3, #32768
	str	r3, [r2, #0]
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L145:
	.align	2
.L144:
	.word	point_pointer
	.size	PointPredictOne, .-PointPredictOne
	.align	2
	.type	PointPredictTwo, %function
PointPredictTwo:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #20
	str	r0, [fp, #-24]
	ldr	r3, .L148
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3, asl #1
	ldr	r3, .L148
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r2
	str	r3, [fp, #-20]
	ldr	r3, .L148
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3, asl #1
	ldr	r3, .L148
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r2
	str	r3, [fp, #-16]
	ldr	r3, .L148
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r3, .L148
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L148
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r0, [fp, #-20]
	ldr	r1, [fp, #-16]
	bl	PointRange
	mov	r3, r0
	str	r3, [r4, #0]
	ldr	r3, .L148
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [r2, #0]
	orr	r3, r3, #32768
	str	r3, [r2, #0]
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L149:
	.align	2
.L148:
	.word	point_pointer
	.size	PointPredictTwo, .-PointPredictTwo
	.align	2
	.type	PointPredictThree, %function
PointPredictThree:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #20
	str	r0, [fp, #-24]
	ldr	r3, .L152
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r2, r3, r2
	ldr	r3, .L152
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	mov	r3, r3, asl #2
	rsb	r3, r3, #0
	add	r2, r2, r3
	ldr	r3, .L152
	ldr	r1, [r3, #12]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	add	r3, r2, r3
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-20]
	ldr	r3, .L152
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r2, r3, r2
	ldr	r3, .L152
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #2
	rsb	r3, r3, #0
	add	r2, r2, r3
	ldr	r3, .L152
	ldr	r1, [r3, #12]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-16]
	ldr	r3, .L152
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r3, .L152
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L152
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r0, [fp, #-20]
	ldr	r1, [fp, #-16]
	bl	PointRange
	mov	r3, r0
	str	r3, [r4, #0]
	ldr	r3, .L152
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [r2, #0]
	orr	r3, r3, #32768
	str	r3, [r2, #0]
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L153:
	.align	2
.L152:
	.word	point_pointer
	.size	PointPredictThree, .-PointPredictThree
	.align	2
	.type	PointPredict, %function
PointPredict:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L155
.L163:
	ldr	r3, .L165
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L156
	ldr	r3, .L165+4
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #15
	cmp	r3, #0
	bne	.L157
	ldr	r3, .L165
	ldr	r2, [r3, #8]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L157
	ldr	r3, .L165
	ldr	r2, [r3, #8]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	bne	.L157
	ldr	r3, .L165
	ldr	r2, [r3, #12]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	bne	.L157
	ldr	r3, .L165
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	bne	.L157
	ldr	r3, .L165+4
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L158
.L157:
	ldr	r3, [fp, #-8]
	mov	r0, r3
	bl	PointPredictOne
	b	.L159
.L158:
	ldr	r3, .L165
	ldr	r2, [r3, #8]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L159
	ldr	r3, .L165
	ldr	r2, [r3, #12]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L160
	ldr	r3, [fp, #-8]
	mov	r0, r3
	bl	PointPredictThree
	b	.L159
.L160:
	ldr	r3, [fp, #-8]
	mov	r0, r3
	bl	PointPredictTwo
.L159:
	ldr	r3, .L165
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L165
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	orr	r2, r2, #8192
	str	r2, [r3, #0]
	ldr	r3, .L165+8
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L165+8
	ldr	r1, [r2, #4]
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	b	.L161
.L156:
	ldr	r3, .L165
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L165+12
	str	r2, [r3, #0]
.L161:
	ldr	r3, .L165
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L162
	ldr	r3, .L165
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L165
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	orr	r2, r2, #4096
	str	r2, [r3, #0]
.L162:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L155:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L163
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L166:
	.align	2
.L165:
	.word	point_pointer
	.word	global_state
	.word	pressure_pointer
	.word	268374015
	.size	PointPredict, .-PointPredict
	.align	2
	.type	PointDistance, %function
PointDistance:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #24
	str	r0, [fp, #-24]
	str	r1, [fp, #-28]
	ldr	r3, .L172
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #1
	cmp	r3, #0
	beq	.L168
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #6
	ldr	r2, .L172+4
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L172+8
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	ldr	r2, .L172+12
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	mul	r2, r1, r3
	ldr	r3, .L172+16
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r2, r3, #63
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #6
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #6
	ldr	r2, .L172+4
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L172+8
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	ldr	r2, .L172+12
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	mul	r2, r1, r3
	ldr	r3, .L172+16
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r2, r3, #63
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #6
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	b	.L169
.L168:
	ldr	r3, .L172
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #2
	cmp	r3, #0
	beq	.L170
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #2]
	mov	r3, r3, asl #6
	ldr	r2, .L172+8
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L172+4
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	ldr	r2, .L172+16
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	mul	r2, r1, r3
	ldr	r3, .L172+12
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r2, r3, #63
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #6
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #2]
	mov	r3, r3, asl #6
	ldr	r2, .L172+8
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L172+4
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	ldr	r2, .L172+16
	ldr	r2, [r2, #0]
	mov	r1, r2, asl #6
	mul	r2, r1, r3
	ldr	r3, .L172+12
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r2, r3, #63
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #6
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	b	.L169
.L170:
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-24]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #19
	mov	r3, r3, lsr #19
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	mul	r3, r1, r2
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L169:
	ldr	r3, [fp, #-8]
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L173:
	.align	2
.L172:
	.word	id_flag
	.word	screen_y_max
	.word	screen_x_max
	.word	drv_num_nokey
	.word	sen_num_nokey
	.size	PointDistance, .-PointDistance
	.align	2
	.type	DistanceInit, %function
DistanceInit:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r3, r3, #12
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L175
.L176:
	ldr	r3, [fp, #-8]
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #4
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L175:
	ldr	r3, [fp, #-12]
	cmp	r3, #99
	ble	.L176
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	DistanceInit, .-DistanceInit
	.align	2
	.type	DistanceMin, %function
DistanceMin:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	mvn	r2, #-2147483648
	str	r2, [r3, #8]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L179
.L183:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L180
.L182:
	ldr	r2, [fp, #-8]
	ldr	r0, [fp, #-12]
	ldr	r1, [fp, #-16]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	add	r2, r3, #2
	mov	r3, #4
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #8]
	cmp	r2, r3
	bcs	.L181
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-16]
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	str	r2, [r3, #4]
	ldr	r2, [fp, #-8]
	ldr	r0, [fp, #-12]
	ldr	r1, [fp, #-16]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	add	r2, r3, #2
	mov	r3, #4
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	str	r2, [r3, #8]
.L181:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L180:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L182
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L179:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L183
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #8]
	cmn	r3, #-2147483647
	bne	.L184
	mov	r3, #0
	b	.L185
.L184:
	mov	r3, #1
.L185:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	DistanceMin, .-DistanceMin
	.align	2
	.type	DistanceIgnore, %function
DistanceIgnore:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L188
.L189:
	ldr	r3, [fp, #-16]
	ldr	r2, [r3, #4]
	ldr	r0, [fp, #-12]
	ldr	r1, [fp, #-16]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	add	r2, r3, #2
	mov	r3, #4
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L188:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L189
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L190
.L191:
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-16]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	add	r2, r3, #2
	mov	r3, #4
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L190:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L191
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	DistanceIgnore, .-DistanceIgnore
	.align	2
	.type	SpeedGet, %function
SpeedGet:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #8
	str	r3, [fp, #-8]
	b	.L194
.L197:
	mov	r2, #256
	ldr	r3, [fp, #-8]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L199
.L195:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L194:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bgt	.L197
	b	.L196
.L199:
	mov	r0, r0	@ nop
.L196:
	ldr	r3, [fp, #-8]
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	SpeedGet, .-SpeedGet
	.align	2
	.type	PointId, %function
PointId:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 464
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, r5, fp, lr}
	add	fp, sp, #12
	sub	sp, sp, #464
	sub	r3, fp, #432
	mov	r0, r3
	bl	DistanceInit
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L201
.L205:
	ldr	r3, .L231
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #128
	cmp	r3, #0
	beq	.L202
	ldr	r3, .L231
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L203
.L202:
	ldr	r1, [fp, #-20]
	ldr	r3, .L231+4
	ldr	r2, [r3, #0]
	ldr	r3, .L231+8
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	b	.L204
.L203:
	ldr	r4, [fp, #-20]
	ldr	r3, .L231
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L231
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	mov	r0, r3
	bl	SpeedGet
	mov	r3, r0
	mov	r2, r3
	ldr	r3, .L231+8
	mov	r1, r4, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
.L204:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L201:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L205
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L206
.L211:
	ldr	r3, .L231
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L231+12
	cmp	r2, r3
	beq	.L227
.L207:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L209
.L210:
	ldr	r4, [fp, #-16]
	ldr	r5, [fp, #-20]
	ldr	r3, [fp, #-16]
	mov	r2, r3, asl #2
	ldr	r3, .L231+16
	add	r2, r2, r3
	ldr	r3, .L231
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r2, r0
	mov	r3, r4
	mov	r3, r3, asl #2
	add	r3, r3, r4
	mov	r3, r3, asl #1
	add	r3, r3, r5
	add	r1, r3, #2
	ldr	r3, .L231+20
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L209:
	ldr	r3, .L231+24
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	bge	.L208
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L210
	b	.L208
.L227:
	mov	r0, r0	@ nop
.L208:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L206:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L211
	ldr	r3, .L231+24
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L228
.L212:
	ldr	r3, .L231+28
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L214
	sub	r3, fp, #432
	mov	r0, r3
	bl	DistanceMin
	mov	r3, r0
	cmp	r3, #0
	beq	.L215
	ldr	r3, .L231
	ldr	r3, [r3, #4]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L216
	ldr	r3, .L231
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r2, r3, #255
	ldr	r1, [fp, #-428]
	ldr	r0, .L231+16
	mov	r3, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r2, r3
	beq	.L216
	sub	r3, fp, #432
	mov	r0, r3
	bl	DistanceIgnore
	b	.L217
.L216:
	ldr	r3, .L231
	ldr	r3, [r3, #0]
	ldr	r1, [fp, #-428]
	ldr	r2, .L231+16
	ldr	r2, [r2, r1, asl #2]
	str	r2, [r3, #0]
	b	.L218
.L215:
	ldr	r3, .L231
	ldr	r3, [r3, #0]
	ldr	r2, .L231+16
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
.L218:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L219
.L220:
	ldr	r2, [fp, #-20]
	ldr	r3, .L231+16
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L219:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L220
.L217:
	ldr	r3, .L231+24
	mov	r2, #1
	str	r2, [r3, #0]
	b	.L226
.L214:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L221
.L225:
	sub	r3, fp, #432
	mov	r0, r3
	bl	DistanceMin
	mov	r3, r0
	cmp	r3, #0
	beq	.L229
.L222:
	ldr	r2, [fp, #-424]
	ldr	r1, [fp, #-432]
	ldr	r3, .L231+8
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	ldr	r1, .L231+32
	ldr	r0, [r1, #0]
	mul	r1, r0, r3
	ldr	r3, .L231+36
	ldr	r3, [r3, #0]
	add	r3, r1, r3
	cmp	r2, r3
	bcs	.L230
.L223:
	ldr	r3, .L231
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-432]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r1, [fp, #-428]
	ldr	r2, .L231+16
	ldr	r2, [r2, r1, asl #2]
	str	r2, [r3, #0]
	ldr	r3, .L231+40
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-432]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r1, [fp, #-428]
	ldr	r2, .L231+44
	ldr	r2, [r2, r1, asl #2]
	str	r2, [r3, #0]
	ldr	r2, [fp, #-428]
	ldr	r3, .L231+16
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	sub	r3, fp, #432
	mov	r0, r3
	bl	DistanceIgnore
	b	.L224
.L230:
	mov	r0, r0	@ nop
.L224:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L221:
	ldr	r3, .L231+24
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	bge	.L226
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L225
	b	.L226
.L228:
	mov	r0, r0	@ nop
	b	.L226
.L229:
	mov	r0, r0	@ nop
.L226:
	sub	sp, fp, #12
	ldmfd	sp!, {r4, r5, fp, lr}
	bx	lr
.L232:
	.align	2
.L231:
	.word	point_pointer
	.word	id_first_coe
	.word	-460
	.word	268374015
	.word	point_now
	.word	-416
	.word	point_num
	.word	global_state
	.word	id_speed_coe
	.word	id_static_coe
	.word	pressure_pointer
	.word	pressure_now
	.size	PointId, .-PointId
	.align	2
	.type	ClearLenPP, %function
ClearLenPP:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L234
.L237:
	ldr	r2, [fp, #-8]
	ldr	r3, .L240
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L239
.L235:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L234:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L237
	b	.L236
.L239:
	mov	r0, r0	@ nop
.L236:
	ldr	r3, [fp, #-8]
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L241:
	.align	2
.L240:
	.word	point_pointer
	.size	ClearLenPP, .-ClearLenPP
	.align	2
	.type	PointNewId, %function
PointNewId:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L243
.L245:
	ldr	r3, .L255
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	bic	r3, r3, #-268435456
	bic	r3, r3, #61440
	ldr	r2, .L255+4
	cmp	r3, r2
	bne	.L244
	ldr	r3, .L255
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
.L244:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L243:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L245
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L246
.L252:
	ldr	r2, [fp, #-8]
	ldr	r3, .L255+8
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L247
	ldr	r2, [fp, #-8]
	ldr	r1, .L255+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L254
.L248:
	mov	r3, #1
	str	r3, [fp, #-12]
	b	.L249
.L251:
	ldr	r3, [fp, #-12]
	sub	r3, r3, #1
	mov	r0, r3
	bl	ClearLenPP
	mov	r3, r0
	cmp	r3, #2
	ble	.L250
	ldr	r3, .L255
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r1, [fp, #-8]
	ldr	r2, .L255+8
	ldr	r2, [r2, r1, asl #2]
	str	r2, [r3, #0]
	ldr	r3, .L255+12
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r1, [fp, #-8]
	ldr	r2, .L255+16
	ldr	r2, [r2, r1, asl #2]
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	ldr	r3, .L255+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	b	.L247
.L250:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L249:
	ldr	r3, [fp, #-12]
	cmp	r3, #10
	ble	.L251
	b	.L247
.L254:
	mov	r0, r0	@ nop
.L247:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L246:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L252
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L256:
	.align	2
.L255:
	.word	point_pointer
	.word	268374015
	.word	point_now
	.word	pressure_pointer
	.word	pressure_now
	.size	PointNewId, .-PointNewId
	.align	2
	.type	PointOrder, %function
PointOrder:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L258
.L262:
	ldr	r3, .L265
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L264
.L259:
	ldr	r3, .L265
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L261
	ldr	r3, .L265
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	bne	.L261
	ldr	r3, .L265+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L261
	ldr	r3, .L265+4
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L260
.L261:
	ldr	r3, .L265
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	ldr	r3, .L265+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	b	.L260
.L264:
	mov	r0, r0	@ nop
.L260:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L258:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L262
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L266:
	.align	2
.L265:
	.word	point_pointer
	.word	filter_able
	.word	pressure_now
	.size	PointOrder, .-PointOrder
	.align	2
	.type	PointCross, %function
PointCross:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L268
.L278:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L269
.L277:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L280
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L281
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L282
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L283
.L271:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r2, [r3, #2]
	ldr	r3, .L284
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	cmp	r2, r3
	bcs	.L273
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r2, [r3, #2]
	ldr	r3, .L284
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	cmp	r2, r3
	bhi	.L274
.L273:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r2, [r3, #2]
	ldr	r3, .L284
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	cmp	r2, r3
	bls	.L272
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r2, [r3, #2]
	ldr	r3, .L284
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	cmp	r2, r3
	bcs	.L272
.L274:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L284
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r2, r3
	bge	.L275
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L284
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r2, r3
	bgt	.L276
.L275:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L284
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r2, r3
	ble	.L272
	ldr	r3, .L284
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L284
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r2, r3
	bge	.L272
.L276:
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-8]
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L284
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-12]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-8]
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L284
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, .L284
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L272
.L280:
	mov	r0, r0	@ nop
	b	.L272
.L281:
	mov	r0, r0	@ nop
	b	.L272
.L282:
	mov	r0, r0	@ nop
	b	.L272
.L283:
	mov	r0, r0	@ nop
.L272:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L269:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	bls	.L277
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L268:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	bls	.L278
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L285:
	.align	2
.L284:
	.word	point_pointer
	.size	PointCross, .-PointCross
	.align	2
	.type	GetPointNum, %function
GetPointNum:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, .L291
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L287
.L289:
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L288
	ldr	r3, .L291
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	ldr	r3, .L291
	str	r2, [r3, #0]
.L288:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L287:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L289
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L292:
	.align	2
.L291:
	.word	point_num
	.size	GetPointNum, .-GetPointNum
	.align	2
	.type	PointDelay, %function
PointDelay:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L294
.L312:
	ldr	r3, .L318
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L295
	ldr	r3, .L318+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L295
	ldr	r2, [fp, #-12]
	ldr	r3, .L318+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, .L318+12
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L314
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r2, r2, r3
	ldrb	r3, [r2, #0]
	orr	r3, r3, #64
	strb	r3, [r2, #0]
	b	.L297
.L295:
	ldr	r3, .L318+12
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L298
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #7
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L298
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L298
	ldr	r3, .L318+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L315
.L299:
	ldr	r3, .L318+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L300
	ldr	r3, .L318+12
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L300
	ldr	r1, [fp, #-12]
	ldr	r3, .L318+4
	ldr	r0, [r3, #0]
	ldr	r3, .L318+16
	ldr	r3, [r3, #0]
	cmp	r3, #10
	movge	r3, #10
	sub	r2, r3, #1
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r0, lsr r3
	and	r3, r3, #255
	and	r3, r3, #7
	ldr	r2, .L318+8
	strb	r3, [r2, r1, asl #2]
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	strb	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	strb	r2, [r3, #0]
	b	.L301
.L300:
	ldr	r1, [fp, #-12]
	ldr	r3, .L318
	ldr	r0, [r3, #0]
	ldr	r3, .L318+16
	ldr	r3, [r3, #0]
	cmp	r3, #10
	movge	r3, #10
	sub	r2, r3, #1
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r0, lsr r3
	and	r3, r3, #255
	and	r3, r3, #7
	ldr	r2, .L318+8
	strb	r3, [r2, r1, asl #2]
	ldr	r1, [fp, #-12]
	ldr	r3, .L318+20
	ldr	r0, [r3, #0]
	ldr	r3, .L318+16
	ldr	r3, [r3, #0]
	cmp	r3, #10
	movge	r3, #10
	sub	r2, r3, #1
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r0, lsr r3
	and	r3, r3, #255
	and	r3, r3, #7
	ldr	r0, .L318+8
	mov	r2, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r2, r1, r2
	strb	r3, [r2, #0]
	ldr	r1, [fp, #-12]
	ldr	r3, .L318+24
	ldr	r0, [r3, #0]
	ldr	r3, .L318+16
	ldr	r3, [r3, #0]
	cmp	r3, #10
	movge	r3, #10
	sub	r2, r3, #1
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r0, lsr r3
	and	r3, r3, #255
	and	r3, r3, #7
	ldr	r0, .L318+8
	mov	r2, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r2, r1, r2
	strb	r3, [r2, #0]
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r1, [fp, #-12]
	ldr	r3, .L318+8
	ldrb	r3, [r3, r1, asl #2]	@ zero_extendqisi2
	cmp	r2, r3
	bls	.L302
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r3, .L318+8
	ldrb	r2, [r3, r2, asl #2]	@ zero_extendqisi2
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strb	r2, [r3, #0]
.L302:
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r3, .L318+8
	ldrb	r2, [r3, r2, asl #2]	@ zero_extendqisi2
	ldr	r0, [fp, #-12]
	ldr	ip, .L318+8
	mov	r3, #1
	mov	r0, r0, asl #2
	add	r0, ip, r0
	add	r3, r0, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	rsb	r3, r3, r2
	and	r2, r3, #255
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strb	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r1, [fp, #-12]
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r2, r3
	bls	.L303
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r0, r2
	add	r3, r2, r3
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r0, .L318+8
	mov	r3, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strb	r2, [r3, #0]
.L303:
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r0, r2
	add	r3, r2, r3
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r0, [fp, #-12]
	ldr	ip, .L318+8
	mov	r3, #2
	mov	r0, r0, asl #2
	add	r0, ip, r0
	add	r3, r0, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	rsb	r3, r3, r2
	and	r2, r3, #255
	ldr	r0, .L318+8
	mov	r3, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strb	r2, [r3, #0]
.L301:
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r2, r2, r3
	ldrb	r3, [r2, #0]
	orr	r3, r3, #128
	strb	r3, [r2, #0]
.L298:
	ldr	r3, .L318+12
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L304
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r2, r2, r3
	ldrb	r3, [r2, #0]
	bic	r3, r3, #128
	strb	r3, [r2, #0]
.L304:
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L305
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #7
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L305
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L306
.L308:
	ldr	r2, [fp, #-8]
	ldr	r3, .L318+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L307
	ldr	r2, [fp, #-8]
	ldr	r3, .L318+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	bne	.L307
	ldr	r2, [fp, #-8]
	ldr	r3, .L318+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #64
	cmp	r3, #0
	bne	.L307
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L306:
	ldr	r2, [fp, #-12]
	ldr	r3, .L318+8
	ldrb	r3, [r3, r2, asl #2]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L308
.L307:
	ldr	r2, [fp, #-12]
	ldr	r3, .L318+8
	ldrb	r3, [r3, r2, asl #2]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L316
.L309:
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r2, r2, r3
	ldrb	r3, [r2, #0]
	orr	r3, r3, #64
	strb	r3, [r2, #0]
.L305:
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L318+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L310
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r2, r2, r3
	ldrb	r3, [r2, #0]
	bic	r3, r3, #64
	strb	r3, [r2, #0]
	b	.L297
.L310:
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L317
.L311:
	ldr	r3, .L318+24
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L297
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L297
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L318+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, [fp, #-12]
	ldr	r0, .L318+8
	mov	r3, #1
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	sub	r1, r3, #1
	ldr	r3, .L318+12
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	cmp	r3, #8
	bhi	.L297
	ldr	r3, [fp, #-12]
	ldr	r0, .L318+8
	mov	r2, #1
	mov	r1, r3, asl #2
	add	r1, r0, r1
	add	r2, r1, r2
	ldrb	r2, [r2, #0]	@ zero_extendqisi2
	sub	r2, r2, #1
	and	r2, r2, #255
	ldr	r0, .L318+8
	mov	r1, #1
	mov	r3, r3, asl #2
	add	r3, r0, r3
	add	r3, r3, r1
	strb	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	ldr	r1, .L318+8
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L297
	ldr	r3, [fp, #-12]
	ldr	r0, .L318+8
	mov	r2, #2
	mov	r1, r3, asl #2
	add	r1, r0, r1
	add	r2, r1, r2
	ldrb	r2, [r2, #0]	@ zero_extendqisi2
	sub	r2, r2, #1
	and	r2, r2, #255
	ldr	r0, .L318+8
	mov	r1, #2
	mov	r3, r3, asl #2
	add	r3, r0, r3
	add	r3, r3, r1
	strb	r2, [r3, #0]
	b	.L297
.L314:
	mov	r0, r0	@ nop
	b	.L297
.L315:
	mov	r0, r0	@ nop
	b	.L297
.L316:
	mov	r0, r0	@ nop
	b	.L297
.L317:
	mov	r0, r0	@ nop
.L297:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L294:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L312
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L319:
	.align	2
.L318:
	.word	report_delay
	.word	delay_key
	.word	point_delay
	.word	point_pointer
	.word	point_num
	.word	report_ahead
	.word	report_delete
	.size	PointDelay, .-PointDelay
	.align	2
	.global	PointMOne
	.type	PointMOne, %function
PointMOne:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bcc	.L321
	mov	r3, #0
	b	.L322
.L321:
	ldr	r3, .L326
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-12]
	cmp	r2, r3
	bcs	.L323
	ldr	r2, [fp, #-12]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r2, r3, lsr #1
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bcs	.L323
	mov	r3, #1
	b	.L322
.L323:
	ldr	r3, .L326
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #1
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bls	.L324
	ldr	r3, [fp, #-12]
	mov	r2, r3, asl #1
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bcs	.L324
	mov	r3, #1
	b	.L322
.L324:
	mov	r3, #0
.L322:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L327:
	.align	2
.L326:
	.word	edge_start
	.size	PointMOne, .-PointMOne
	.align	2
	.global	PointMenu
	.type	PointMenu, %function
PointMenu:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	ldr	r3, .L351
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L343
.L329:
	ldr	r3, .L351+4
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L344
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L345
	ldr	r3, .L351+4
	ldr	r3, [r3, #8]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L346
.L332:
	mov	r3, #1
	str	r3, [fp, #-8]
	b	.L333
.L335:
	ldr	r3, .L351+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L347
.L334:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L333:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L335
	ldr	r3, .L351+8
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L348
	ldr	r3, .L351+8
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L349
.L337:
	ldr	r3, .L351+4
	ldr	r2, [r3, #0]
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	add	r3, r3, #4
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r2, r0
	ldr	r3, .L351+12
	ldr	r3, [r3, #0]
	cmp	r3, #36
	movcc	r3, #36
	cmp	r2, r3
	bcc	.L350
.L338:
	ldr	r3, .L351+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	ldrh	r3, [r3, #2]
	mov	r0, r2
	mov	r1, r3
	bl	PointMOne
	mov	r3, r0
	cmp	r3, #0
	beq	.L339
	ldr	r3, .L351+16
	ldr	r3, [r3, #4]
	mov	r2, #1
	strh	r2, [r3, #2]	@ movhi
.L339:
	ldr	r3, .L351+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r2
	mov	r1, r3
	bl	PointMOne
	mov	r3, r0
	cmp	r3, #0
	beq	.L340
	ldr	r3, .L351+16
	ldr	r2, [r3, #4]
	ldr	r3, [r2, #0]
	bic	r3, r3, #4080
	bic	r3, r3, #14
	orr	r3, r3, #1
	str	r3, [r2, #0]
.L340:
	ldr	r3, .L351+20
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, .L351+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #2]
	rsb	r2, r3, r2
	ldr	r3, .L351+20
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	PointMOne
	mov	r3, r0
	cmp	r3, #0
	beq	.L341
	ldr	r3, .L351+16
	ldr	r3, [r3, #4]
	ldr	r2, .L351+20
	ldr	r2, [r2, #0]
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	mov	r2, r2, asl #6
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	sub	r2, r2, #1
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
.L341:
	ldr	r3, .L351+24
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, .L351+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r2, r3, r2
	ldr	r3, .L351+24
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, .L351+4
	ldr	r3, [r3, #4]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	PointMOne
	mov	r3, r0
	cmp	r3, #0
	beq	.L342
	ldr	r3, .L351+16
	ldr	r2, [r3, #4]
	ldr	r3, .L351+24
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #6
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	sub	r3, r3, #1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L342
.L343:
	mov	r0, r0	@ nop
	b	.L342
.L344:
	mov	r0, r0	@ nop
	b	.L342
.L345:
	mov	r0, r0	@ nop
	b	.L342
.L346:
	mov	r0, r0	@ nop
	b	.L342
.L347:
	mov	r0, r0	@ nop
	b	.L342
.L348:
	mov	r0, r0	@ nop
	b	.L342
.L349:
	mov	r0, r0	@ nop
	b	.L342
.L350:
	mov	r0, r0	@ nop
.L342:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L352:
	.align	2
.L351:
	.word	edge_start
	.word	point_pointer
	.word	point_delay
	.word	shake_min
	.word	point_report
	.word	drv_num_nokey
	.word	sen_num_nokey
	.size	PointMenu, .-PointMenu
	.align	2
	.type	FilterOne, %function
FilterOne:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 32
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #32
	str	r0, [fp, #-24]
	str	r1, [fp, #-28]
	str	r2, [fp, #-32]
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-8]
	ldr	r3, .L366
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L366+4
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-24]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, .L366
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L364
.L354:
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	ble	.L365
.L356:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L357
.L358:
	ldr	r2, [fp, #-16]
	ldr	r3, .L366
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	ldr	r2, [fp, #-16]
	mov	r2, r2, asl #2
	ldr	r1, [fp, #-32]
	add	r2, r1, r2
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r1, [fp, #-16]
	ldr	r3, .L366+4
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	mov	r1, r3
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	ldr	r0, [fp, #-28]
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r3, r2, r3
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, .L366
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, [fp, #-16]
	mov	r2, r2, asl #2
	ldr	r1, [fp, #-32]
	add	r2, r1, r2
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r1, [fp, #-16]
	ldr	r3, .L366+4
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	ldr	r0, [fp, #-28]
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r3, r2, r3
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L357:
	ldr	r3, [fp, #-16]
	cmp	r3, #7
	ble	.L358
	ldr	r3, [fp, #-36]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r2, r3
	ldr	r3, [fp, #-12]
	add	r3, r2, r3
	mov	r0, r3
	ldr	r1, [fp, #-36]
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-36]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r2, r3
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	mov	r0, r3
	ldr	r1, [fp, #-36]
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bge	.L359
	mov	r3, #0
	str	r3, [fp, #-12]
.L359:
	ldr	r2, [fp, #-12]
	ldr	r3, .L366+8
	cmp	r2, r3
	ble	.L360
	ldr	r3, .L366+8
	str	r3, [fp, #-12]
.L360:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bge	.L361
	mov	r3, #0
	str	r3, [fp, #-8]
.L361:
	ldr	r2, [fp, #-8]
	ldr	r3, .L366+12
	cmp	r2, r3
	ble	.L362
	ldr	r3, .L366+12
	str	r3, [fp, #-8]
.L362:
	ldr	r3, .L366
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [fp, #-12]
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L366
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L363
.L364:
	mov	r0, r0	@ nop
	b	.L363
.L365:
	mov	r0, r0	@ nop
.L363:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L367:
	.align	2
.L366:
	.word	point_report
	.word	point_stretch
	.word	65535
	.word	4095
	.size	FilterOne, .-FilterOne
	.align	2
	.type	FilterSpeed, %function
FilterSpeed:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #12
	str	r0, [fp, #-16]
	ldr	r3, .L370
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L370
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	mov	r0, r3
	bl	Sqrt
	mov	r4, r0
	ldr	r3, .L370
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L370
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	mov	r0, r3
	bl	Sqrt
	mov	r3, r0
	add	r3, r4, r3
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r0, r3
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L371:
	.align	2
.L370:
	.word	point_stretch
	.size	FilterSpeed, .-FilterSpeed
	.align	2
	.type	MedianSpeedOver, %function
MedianSpeedOver:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #24
	str	r0, [fp, #-24]
	str	r1, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-28]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	sub	r3, r3, #1
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	blt	.L373
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L374
.L373:
	mov	r3, #1
	b	.L375
.L374:
	ldr	r3, [fp, #-28]
	ldr	r2, .L382
	ldrb	r3, [r2, r3]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, [fp, #-28]
	ldr	r1, .L382
	ldrb	r3, [r1, r3]	@ zero_extendqisi2
	mov	r1, r3
	mul	r3, r1, r2
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L376
.L379:
	ldr	r2, [fp, #-16]
	ldr	r3, .L382+4
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-16]
	add	r1, r3, #1
	ldr	r3, .L382+4
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r2, r0
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bls	.L377
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L377:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L376:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bgt	.L378
	ldr	r3, [fp, #-16]
	cmp	r3, #29
	ble	.L379
.L378:
	ldr	r3, [fp, #-8]
	cmp	r3, #1
	ble	.L380
	mov	r3, #1
	b	.L375
.L380:
	mov	r3, #0
.L375:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L383:
	.align	2
.L382:
	.word	median_dis
	.word	point_stretch
	.size	MedianSpeedOver, .-MedianSpeedOver
	.align	2
	.type	PointMedian, %function
PointMedian:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 96
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #96
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L385
.L395:
	ldr	r2, [fp, #-16]
	ldr	r3, .L397
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #2
	bgt	.L386
	mov	r3, #3
	str	r3, [fp, #-8]
	b	.L387
.L386:
	ldr	r2, [fp, #-16]
	ldr	r3, .L397
	ldr	r3, [r3, r2, asl #2]
	add	r3, r3, #2
	str	r3, [fp, #-8]
.L387:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L388
	mov	r3, #9
	str	r3, [fp, #-8]
.L388:
	ldr	r3, [fp, #-8]
	orr	r3, r3, #1
	str	r3, [fp, #-8]
	b	.L389
.L394:
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-8]
	bl	MedianSpeedOver
	mov	r3, r0
	cmp	r3, #0
	beq	.L390
	ldr	r3, [fp, #-8]
	sub	r3, r3, #2
	str	r3, [fp, #-8]
	b	.L389
.L390:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L391
.L392:
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r3, .L397+4
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	mvn	r3, #51
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r3, .L397+4
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	mvn	r3, #91
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L391:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	blt	.L392
	sub	r3, fp, #56
	mov	r0, r3
	ldr	r1, [fp, #-8]
	bl	SortBubble
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r1, [fp, #-8]
	bl	SortBubble
	ldr	r3, .L397+8
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	mov	r1, r3
	mvn	r3, #51
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r2, #2]	@ movhi
	ldr	r3, .L397+8
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	mov	r1, r3
	mvn	r3, #91
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L393
.L389:
	ldr	r3, [fp, #-8]
	cmp	r3, #2
	bgt	.L394
.L393:
	ldr	r2, [fp, #-16]
	ldr	r3, .L397
	ldr	r1, [fp, #-8]
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L385:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L395
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L398:
	.align	2
.L397:
	.word	filter_deep
	.word	point_stretch
	.word	point_report
	.size	PointMedian, .-PointMedian
	.align	2
	.type	PointFilter, %function
PointFilter:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 104
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #104
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L400
.L401:
	ldr	r3, .L429
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L429+4
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-16]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L400:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L401
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L402
.L408:
	ldr	r3, .L429
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L403
	ldr	r3, .L429
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L403
	mov	r3, #1
	str	r3, [fp, #-12]
	b	.L404
.L405:
	ldr	r2, [fp, #-12]
	ldr	r3, .L429
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L429+4
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-16]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L404:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L405
	mov	r3, #1
	str	r3, [fp, #-12]
	b	.L406
.L407:
	ldr	r2, [fp, #-12]
	ldr	r3, .L429+4
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L429+4
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-16]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L406:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L407
.L403:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L402:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L408
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	blt	.L409
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	cmp	r3, #1
	ble	.L428
.L409:
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	cmp	r3, #1
	ble	.L411
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L412
.L415:
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	mov	r2, r3
	ldr	r3, .L429+12
	ldr	r0, [r3, r2, asl #2]
	ldr	r2, [fp, #-16]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	mvn	r3, #67
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r2, r3, #2
	ldr	r3, .L429+12
	ldr	r0, [r3, r2, asl #2]
	ldr	r2, [fp, #-16]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	mvn	r3, #99
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r2, [fp, #-16]
	mvn	r3, #67
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #127
	ble	.L413
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-16]
	mvn	r3, #67
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3, asl #24
	mvn	r3, r3, lsr #24
	mov	r2, r3
	mvn	r3, #67
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
.L413:
	ldr	r2, [fp, #-16]
	mvn	r3, #99
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #127
	ble	.L414
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-16]
	mvn	r3, #99
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3, asl #24
	mvn	r3, r3, lsr #24
	mov	r2, r3
	mvn	r3, #99
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
.L414:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L412:
	ldr	r3, [fp, #-16]
	cmp	r3, #7
	ble	.L415
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L416
.L417:
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	sub	r1, fp, #72
	sub	r2, fp, #104
	ldr	r0, [fp, #-16]
	bl	FilterOne
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L416:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L417
	b	.L427
.L411:
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	cmn	r3, #1
	bne	.L418
	bl	PointMedian
	b	.L427
.L418:
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bge	.L427
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L419
.L420:
	ldr	r3, [fp, #-16]
	add	r1, r3, #1
	ldr	r3, [fp, #-16]
	ldr	r2, .L429+16
	ldrb	r3, [r2, r3]	@ zero_extendqisi2
	mov	r2, r3
	mvn	r3, #35
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L419:
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	ble	.L420
	ldr	r3, .L429+16
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3, asl #1
	ldr	r3, .L429+16
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	rsb	r3, r3, r2
	str	r3, [fp, #-40]
	ldr	r3, .L429+16
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	mov	r3, r3, lsr #1
	and	r3, r3, #255
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L421
.L426:
	ldr	r3, .L429
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L422
	ldr	r2, [fp, #-16]
	ldr	r3, .L429+20
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	b	.L423
.L422:
	ldr	r0, [fp, #-16]
	bl	FilterSpeed
	mov	r3, r0
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	ble	.L424
	ldr	r2, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r2, asl #2]
	sub	r2, r3, #1
	mvn	r3, #35
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L424
	ldr	r3, [fp, #-16]
	ldr	r2, .L429+20
	ldr	r2, [r2, r3, asl #2]
	sub	r1, r2, #1
	ldr	r2, .L429+20
	str	r1, [r2, r3, asl #2]
	b	.L425
.L424:
	ldr	r2, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #2
	bgt	.L425
	ldr	r2, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r2, asl #2]
	add	r2, r3, #3
	mvn	r3, #35
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	ble	.L425
	ldr	r3, [fp, #-16]
	ldr	r2, .L429+20
	ldr	r2, [r2, r3, asl #2]
	add	r1, r2, #1
	ldr	r2, .L429+20
	str	r1, [r2, r3, asl #2]
.L425:
	ldr	r2, .L429+24
	ldr	r1, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r1, asl #2]
	mov	r3, r3, asl #5
	add	r1, r2, r3
	ldr	r2, .L429+28
	ldr	r0, [fp, #-16]
	ldr	r3, .L429+20
	ldr	r3, [r3, r0, asl #2]
	mov	r3, r3, asl #5
	add	r2, r2, r3
	ldr	r3, .L429+8
	ldr	r3, [r3, #0]
	rsb	r3, r3, #0
	ldr	r0, [fp, #-16]
	bl	FilterOne
.L423:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L421:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	ble	.L426
	b	.L427
.L428:
	mov	r0, r0	@ nop
.L427:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L430:
	.align	2
.L429:
	.word	point_report
	.word	point_stretch
	.word	filter_able
	.word	filter_coe
	.word	median_dis
	.word	filter_deep
	.word	ps_coe
	.word	pr_coe
	.size	PointFilter, .-PointFilter
	.align	2
	.type	KeyMap, %function
KeyMap:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L437
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L432
.L435:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	cmp	r1, r3
	bcc	.L433
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r1, r3
	bhi	.L433
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #4]
	mov	r3, r3, lsr #16
	cmp	r1, r3
	bcc	.L433
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #4]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r1, r3
	bhi	.L433
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #8]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-20]
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, [fp, #-16]
	str	r3, [r2, #0]
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-12]
	add	r3, r2, r3
	ldr	r3, [r3, #8]
	b	.L434
.L433:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L432:
	ldr	r3, [fp, #-8]
	cmp	r3, #7
	ble	.L435
	mov	r3, #0
.L434:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L438:
	.align	2
.L437:
	.word	key_range_array
	.size	KeyMap, .-KeyMap
	.align	2
	.type	ScreenResolution, %function
ScreenResolution:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	bne	.L440
	ldr	r2, [fp, #-12]
	ldr	r3, .L463
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, .L463
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	add	r3, r3, #2048
	add	r2, r3, #4080
	add	r2, r2, #15
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #12
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-8]
	ldr	r3, .L463+4
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, .L463+4
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	add	r3, r3, #2048
	add	r2, r3, #4080
	add	r2, r2, #15
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #12
	str	r3, [fp, #-8]
.L440:
	ldr	r3, [fp, #-12]
	ldr	r2, .L463+8
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L463+12
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-8]
	ldr	r2, .L463+16
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L463+20
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	bne	.L441
	ldr	r3, .L463+24
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	bne	.L442
	ldr	r3, .L463+28
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L443
	ldr	r3, .L463+28
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L444
.L443:
	ldr	r3, .L463+28
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	ble	.L445
	mov	r3, #0
	b	.L446
.L445:
	ldr	r3, .L463+28
	ldr	r2, [r3, #4]
	ldr	r3, .L463+8
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	bgt	.L447
	ldr	r3, .L463+8
	ldr	r2, [r3, #0]
	ldr	r3, .L463+28
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L447
	mov	r3, #0
	b	.L446
.L447:
	ldr	r3, .L463+28
	ldr	r2, [r3, #4]
	ldr	r3, .L463+8
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	blt	.L444
	ldr	r3, .L463+28
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L444
	mov	r3, #0
	b	.L446
.L444:
	ldr	r3, .L463+32
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L448
	ldr	r3, .L463+32
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L442
.L448:
	ldr	r3, .L463+32
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	ble	.L449
	mov	r3, #0
	b	.L446
.L449:
	ldr	r3, .L463+32
	ldr	r2, [r3, #4]
	ldr	r3, .L463+16
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	bgt	.L450
	ldr	r3, .L463+16
	ldr	r2, [r3, #0]
	ldr	r3, .L463+32
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L450
	mov	r3, #0
	b	.L446
.L450:
	ldr	r3, .L463+32
	ldr	r2, [r3, #4]
	ldr	r3, .L463+16
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	blt	.L442
	ldr	r3, .L463+32
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L442
	mov	r3, #0
	b	.L446
.L442:
	ldr	r3, .L463+36
	ldr	r3, [r3, #8]
	mov	r2, r3
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	blt	.L451
	ldr	r3, .L463+36
	ldr	r3, [r3, #8]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L451:
	ldr	r3, .L463+8
	ldr	r2, [r3, #0]
	ldr	r3, .L463+36
	ldr	r3, [r3, #12]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bgt	.L452
	ldr	r3, .L463+8
	ldr	r2, [r3, #0]
	ldr	r3, .L463+36
	ldr	r3, [r3, #12]
	rsb	r3, r3, r2
	sub	r3, r3, #1
	str	r3, [fp, #-12]
.L452:
	ldr	r3, .L463+36
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	blt	.L453
	ldr	r3, .L463+36
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L453:
	ldr	r3, .L463+16
	ldr	r2, [r3, #0]
	ldr	r3, .L463+36
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bgt	.L454
	ldr	r3, .L463+16
	ldr	r2, [r3, #0]
	ldr	r3, .L463+36
	ldr	r3, [r3, #4]
	rsb	r3, r3, r2
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L454:
	ldr	r3, .L463+40
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #2
	cmp	r3, #0
	beq	.L455
	ldr	r3, .L463+8
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	rsb	r3, r3, r2
	str	r3, [fp, #-12]
.L455:
	ldr	r3, .L463+40
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #4
	cmp	r3, #0
	beq	.L456
	ldr	r3, .L463+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
.L456:
	ldr	r3, .L463+40
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #8
	cmp	r3, #0
	beq	.L462
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	eor	r3, r2, r3
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-12]
	eor	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	eor	r3, r2, r3
	str	r3, [fp, #-12]
	b	.L458
.L441:
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bge	.L459
	mov	r3, #0
	str	r3, [fp, #-12]
.L459:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bge	.L460
	mov	r3, #0
	str	r3, [fp, #-8]
.L460:
	ldr	r3, .L463+44
	ldr	r3, [r3, #0]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L458
	sub	r2, fp, #8
	sub	r3, fp, #12
	mov	r0, r2
	mov	r1, r3
	bl	KeyMap
	mov	r3, r0
	cmp	r3, #0
	bne	.L458
	mov	r3, #0
	b	.L446
.L462:
	mov	r0, r0	@ nop
.L458:
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #16
	ldr	r2, .L463+48
	and	r2, r3, r2
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
.L446:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L464:
	.align	2
.L463:
	.word	match_y
	.word	match_x
	.word	screen_y_max
	.word	sen_num_nokey
	.word	screen_x_max
	.word	drv_num_nokey
	.word	id_flag
	.word	ignore_y
	.word	ignore_x
	.word	edge_cut
	.word	global_flag
	.word	key_map_able
	.word	268369920
	.size	ScreenResolution, .-ScreenResolution
	.align	2
	.type	PointReport, %function
PointReport:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #60
	str	r0, [fp, #-64]
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r3, .L507
	ldr	r2, [r3, #0]
	ldr	r3, .L507+4
	ldr	r3, [r3, #0]
	cmp	r2, r3
	ble	.L466
	ldr	r3, .L507+8
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #1
	cmp	r3, #0
	beq	.L466
	ldr	r3, .L507
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-64]
	mov	r2, #0
	str	r2, [r3, #120]
	b	.L497
.L466:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L468
.L469:
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L468:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L469
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r3, .L507+8
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #64
	cmp	r3, #0
	beq	.L470
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L471
.L477:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L498
.L472:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #8
	bhi	.L499
.L474:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	add	r2, r3, #1
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #64
	cmp	r3, #0
	beq	.L500
.L475:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L473
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [r2, #0]
	orr	r3, r3, #16384
	str	r3, [r2, #0]
	ldr	r4, [fp, #-20]
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	bl	ScreenResolution
	mov	r2, r0
	mvn	r3, #47
	mov	r1, r4, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L473
	ldr	r1, [fp, #-20]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	mov	r3, r3, asl #28
	orr	r2, r2, r3
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L473
.L498:
	mov	r0, r0	@ nop
	b	.L473
.L499:
	mov	r0, r0	@ nop
	b	.L473
.L500:
	mov	r0, r0	@ nop
.L473:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L471:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	bgt	.L476
	ldr	r3, .L507+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	blt	.L477
.L476:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L478
.L484:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L501
.L479:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #9
	bhi	.L502
.L481:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L503
.L482:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #64
	cmp	r3, #0
	bne	.L480
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [r2, #0]
	orr	r3, r3, #16384
	str	r3, [r2, #0]
	ldr	r4, [fp, #-20]
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	bl	ScreenResolution
	mov	r2, r0
	mvn	r3, #47
	mov	r1, r4, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L480
	ldr	r1, [fp, #-20]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	mov	r3, r3, asl #28
	orr	r2, r2, r3
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L480
.L501:
	mov	r0, r0	@ nop
	b	.L480
.L502:
	mov	r0, r0	@ nop
	b	.L480
.L503:
	mov	r0, r0	@ nop
.L480:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L478:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	bgt	.L485
	ldr	r3, .L507+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	blt	.L484
	b	.L485
.L470:
	mov	r3, #0
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L486
.L490:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L504
.L487:
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #9
	bhi	.L505
.L489:
	ldr	r4, [fp, #-16]
	ldr	r2, [fp, #-20]
	ldr	r1, .L507+12
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r2, r3
	ldr	r3, .L507+16
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	bl	ScreenResolution
	mov	r2, r0
	mvn	r3, #47
	mov	r1, r4, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r2, [fp, #-16]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L488
	ldr	r3, [fp, #-16]
	mov	r1, r3
	mov	r2, r3
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	mov	r3, r3, asl #28
	orr	r2, r2, r3
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L488
.L504:
	mov	r0, r0	@ nop
	b	.L488
.L505:
	mov	r0, r0	@ nop
.L488:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L486:
	ldr	r3, .L507+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-20]
	cmp	r2, r3
	bge	.L485
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L490
.L485:
	mov	r3, #0
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L491
.L494:
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L506
.L492:
	ldr	r2, [fp, #-16]
	ldr	r1, [fp, #-20]
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L507+20
	str	r1, [r3, r2, asl #2]
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	ldr	r2, [fp, #-64]
	str	r3, [r2, r1, asl #2]
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-20]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	ldr	r2, [fp, #-64]
	add	r1, r1, #10
	str	r3, [r2, r1, asl #2]
	ldr	r2, [fp, #-16]
	ldr	r1, [fp, #-20]
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #28
	mov	r1, r3
	ldr	r3, [fp, #-64]
	add	r2, r2, #20
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-16]
	ldr	r1, [fp, #-20]
	ldr	r3, .L507+24
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, .L507+28
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L493
.L506:
	mov	r0, r0	@ nop
.L493:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L491:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L494
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-20]
	b	.L495
.L496:
	ldr	r2, [fp, #-20]
	ldr	r3, .L507+20
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-20]
	ldr	r3, .L507+28
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L495:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L496
	ldr	r3, .L507
	ldr	r2, [fp, #-16]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-64]
	ldr	r2, [fp, #-16]
	str	r2, [r3, #120]
.L497:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L508:
	.align	2
.L507:
	.word	point_num
	.word	point_num_max
	.word	global_flag
	.word	point_delay
	.word	point_report
	.word	point_now
	.word	pressure_report
	.word	pressure_now
	.size	PointReport, .-PointReport
	.global	__aeabi_uidiv
	.align	2
	.type	PointEdge, %function
PointEdge:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 160
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #164
	ldr	r3, .L575
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L565
	ldr	r3, .L575+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L566
.L511:
	mov	r3, #0
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L513
.L517:
	ldr	r3, .L575+8
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L514
	ldr	r1, [fp, #-28]
	ldr	r2, [fp, #-28]
	ldr	r3, .L575+12
	ldr	r3, [r3, r2, asl #2]
	mov	r2, r3
	mvn	r3, #155
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	b	.L515
.L514:
	ldr	r1, [fp, #-28]
	ldr	r2, [fp, #-28]
	ldr	r3, .L575+16
	ldr	r3, [r3, r2, asl #2]
	mov	r2, r3
	mvn	r3, #155
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
.L515:
	ldr	r2, [fp, #-28]
	mvn	r3, #155
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L516
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L516:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L513:
	ldr	r3, [fp, #-28]
	cmp	r3, #31
	ble	.L517
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L567
.L518:
	sub	r3, fp, #168
	str	r3, [fp, #-32]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L519
.L530:
	ldr	r3, .L575+20
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #4
	cmp	r3, #0
	bne	.L568
.L520:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #12
	ldr	r2, [r3, r2, asl #3]
	ldr	r3, .L575+4
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	cmp	r2, r3
	bgt	.L522
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #4
	ldr	r2, [r3, r2, asl #3]
	ldr	r3, .L575
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	cmp	r2, r3
	bgt	.L522
	ldr	r3, .L575+20
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #8
	cmp	r3, #0
	beq	.L523
.L522:
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L524
.L529:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L525
	ldr	r4, [fp, #-28]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, r2, asl #3]
	mov	r2, r3
	ldr	r3, .L575+24
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, asl #6
	ldr	r3, .L575
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, [fp, #-32]
	str	r2, [r3, r4, asl #3]
.L525:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #4
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L526
	ldr	r4, [fp, #-28]
	ldr	r3, .L575
	ldr	r2, [r3, #0]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #4
	ldr	r3, [r3, r1, asl #3]
	rsb	r3, r3, r2
	mov	r2, r3
	ldr	r3, .L575+24
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, asl #6
	ldr	r3, .L575
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r1, r3
	ldr	r3, [fp, #-32]
	add	r2, r4, #4
	str	r1, [r3, r2, asl #3]
.L526:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #8
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L527
	ldr	r4, [fp, #-28]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #8
	ldr	r3, [r3, r2, asl #3]
	mov	r2, r3
	ldr	r3, .L575+28
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, asl #6
	ldr	r3, .L575+4
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r1, r3
	ldr	r3, [fp, #-32]
	add	r2, r4, #8
	str	r1, [r3, r2, asl #3]
.L527:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #12
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L528
	ldr	r4, [fp, #-28]
	ldr	r3, .L575+4
	ldr	r2, [r3, #0]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #12
	ldr	r3, [r3, r1, asl #3]
	rsb	r3, r3, r2
	mov	r2, r3
	ldr	r3, .L575+28
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, asl #6
	ldr	r3, .L575+4
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r1, r3
	ldr	r3, [fp, #-32]
	add	r2, r4, #12
	str	r1, [r3, r2, asl #3]
.L528:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L524:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L529
	b	.L521
.L523:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L519:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L530
	b	.L521
.L568:
	mov	r0, r0	@ nop
.L521:
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L531
.L563:
	ldr	r2, [fp, #-24]
	ldr	r3, .L575+32
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L569
	ldr	r2, [fp, #-24]
	ldr	r1, .L575+32
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L570
.L533:
	ldr	r2, [fp, #-24]
	ldr	r1, .L575+32
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-24]
	ldr	r2, .L575+32
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-40]
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L535
.L539:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #8
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L571
.L536:
	ldr	r2, [fp, #-36]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #8
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L538
	ldr	r2, [fp, #-40]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #8
	ldr	r1, [r3, r1, asl #3]
	ldr	r3, [fp, #-36]
	rsb	r1, r3, r1
	ldr	r3, [fp, #-28]
	ldr	ip, [fp, #-32]
	add	r0, r3, #8
	mov	r3, #4
	mov	r0, r0, asl #3
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #8
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-36]
.L538:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L535:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L539
	b	.L537
.L571:
	mov	r0, r0	@ nop
.L537:
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-40]
	rsb	r3, r3, r2
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bgt	.L540
	mov	r3, #1
	str	r3, [fp, #-16]
.L540:
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bgt	.L541
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-16]
.L541:
	mov	r3, #0
	str	r3, [fp, #-40]
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r2
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L542
.L546:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #12
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L572
.L543:
	ldr	r2, [fp, #-36]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #12
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L545
	ldr	r2, [fp, #-40]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #12
	ldr	r1, [r3, r1, asl #3]
	ldr	r3, [fp, #-36]
	rsb	r1, r3, r1
	ldr	r3, [fp, #-28]
	ldr	ip, [fp, #-32]
	add	r0, r3, #12
	mov	r3, #4
	mov	r0, r0, asl #3
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #12
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-36]
.L545:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L542:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L546
	b	.L544
.L572:
	mov	r0, r0	@ nop
.L544:
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r1, [fp, #-40]
	ldr	r3, [fp, #-36]
	rsb	r3, r3, r1
	add	r3, r2, r3
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bgt	.L547
	mov	r3, #1
	str	r3, [fp, #-16]
.L547:
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bgt	.L548
	ldr	r3, .L575+28
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-16]
.L548:
	mov	r3, #0
	str	r3, [fp, #-40]
	ldr	r3, [fp, #-20]
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L549
.L553:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L573
.L550:
	ldr	r2, [fp, #-36]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L552
	ldr	r2, [fp, #-40]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r1, [r3, r1, asl #3]
	ldr	r3, [fp, #-36]
	rsb	r1, r3, r1
	ldr	r0, [fp, #-28]
	ldr	ip, [fp, #-32]
	mov	r3, #4
	mov	r0, r0, asl #3
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-36]
.L552:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L549:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L553
	b	.L551
.L573:
	mov	r0, r0	@ nop
.L551:
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-40]
	rsb	r3, r3, r2
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bgt	.L554
	mov	r3, #1
	str	r3, [fp, #-20]
.L554:
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L555
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-20]
.L555:
	mov	r3, #0
	str	r3, [fp, #-40]
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L556
.L560:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #4
	ldr	r3, [r3, r2, asl #3]
	cmp	r3, #0
	beq	.L574
.L557:
	ldr	r2, [fp, #-36]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #4
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L559
	ldr	r2, [fp, #-40]
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r1, r1, #4
	ldr	r1, [r3, r1, asl #3]
	ldr	r3, [fp, #-36]
	rsb	r1, r3, r1
	ldr	r3, [fp, #-28]
	ldr	ip, [fp, #-32]
	add	r0, r3, #4
	mov	r3, #4
	mov	r0, r0, asl #3
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	add	r1, r3, #127
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #7
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-32]
	add	r2, r2, #4
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-36]
.L559:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L556:
	ldr	r3, [fp, #-28]
	cmp	r3, #3
	ble	.L560
	b	.L558
.L574:
	mov	r0, r0	@ nop
.L558:
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r1, [fp, #-40]
	ldr	r3, [fp, #-36]
	rsb	r3, r3, r1
	add	r3, r2, r3
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bgt	.L561
	mov	r3, #1
	str	r3, [fp, #-20]
.L561:
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L562
	ldr	r3, .L575+24
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-20]
.L562:
	ldr	r1, [fp, #-24]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r0, .L575+32
	mov	r3, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	ldr	r0, [fp, #-24]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L575+32
	mov	r3, r0, asl #2
	add	r2, r2, r3
	ldrh	r3, [r2, #0]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #0]	@ movhi
	b	.L534
.L569:
	mov	r0, r0	@ nop
	b	.L534
.L570:
	mov	r0, r0	@ nop
.L534:
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L531:
	ldr	r3, [fp, #-24]
	cmp	r3, #9
	ble	.L563
	b	.L564
.L565:
	mov	r0, r0	@ nop
	b	.L564
.L566:
	mov	r0, r0	@ nop
	b	.L564
.L567:
	mov	r0, r0	@ nop
.L564:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L576:
	.align	2
.L575:
	.word	screen_x_max
	.word	screen_y_max
	.word	global_state
	.word	stretch_active
	.word	stretch_array
	.word	id_flag
	.word	drv_num_nokey
	.word	sen_num_nokey
	.word	point_now
	.size	PointEdge, .-PointEdge
	.align	2
	.type	PointStretch, %function
PointStretch:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 104
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, r5, fp, lr}
	add	fp, sp, #12
	sub	sp, sp, #104
	ldr	r3, .L655
	str	r3, [fp, #-40]
	mov	r3, #8
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L578
.L579:
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+64
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L578:
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	ble	.L579
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L580
.L601:
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L581
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L582
.L581:
	mov	r2, #1
	ldr	r3, [fp, #-36]
	mov	r3, r2, asl r3
	mvn	r3, r3
	mov	r2, r3
	ldr	r3, .L655+52
	ldr	r3, [r3, #0]
	and	r2, r2, r3
	ldr	r3, .L655+52
	str	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	bne	.L647
	ldr	r3, .L655+4
	mov	r2, #0
	str	r2, [r3, #0]
	b	.L584
.L582:
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	bne	.L585
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L586
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L586
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L655+4
	str	r2, [r3, #8]
	ldr	r3, .L655+4
	ldrh	r3, [r3, #10]
	mov	r2, r3
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #24
	cmp	r2, r3
	bcs	.L587
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #24
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+4
	strh	r2, [r3, #10]	@ movhi
.L587:
	ldr	r3, .L655+4
	ldrh	r3, [r3, #10]
	mov	r2, r3
	ldr	r3, .L655+12
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	and	r3, r3, #255
	rsb	r3, r3, r1
	cmp	r2, r3
	bls	.L588
	ldr	r3, .L655+12
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #6
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	and	r3, r3, #255
	rsb	r3, r3, r2
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+4
	strh	r2, [r3, #10]	@ movhi
.L588:
	ldr	r3, .L655+4
	ldrh	r3, [r3, #8]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	and	r3, r3, #255
	cmp	r2, r3
	bcs	.L589
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	and	r3, r3, #255
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L655+4
	ldrh	r3, [r2, #8]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #8]	@ movhi
.L589:
	ldr	r3, .L655+4
	ldrh	r3, [r3, #8]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L655+16
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	and	r3, r3, #255
	rsb	r3, r3, r1
	cmp	r2, r3
	bls	.L590
	ldr	r3, .L655+16
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #6
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+8
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	and	r3, r3, #255
	rsb	r3, r3, r2
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, .L655+4
	ldrh	r3, [r2, #8]	@ movhi
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	strh	r3, [r2, #8]	@ movhi
.L590:
	ldr	r3, .L655+4
	ldr	r2, [r3, #8]
	ldr	r3, .L655+68
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	beq	.L586
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	ldr	r1, .L655+20
	bl	PointDistance
	mov	r2, r0
	ldr	r3, .L655+4
	str	r2, [r3, #4]
	ldr	r3, .L655+4
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L586
	ldr	r3, .L655+4
	mov	r2, #4096
	str	r2, [r3, #0]
.L586:
	ldr	r3, .L655+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L585
	ldr	r3, .L655+4
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L585
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	ldr	r1, .L655+20
	bl	PointDistance
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, .L655+4
	ldr	r2, [r3, #4]
	ldr	r3, .L655+24
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, lsr #7
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bhi	.L591
	ldr	r3, .L655+4
	mov	r2, #0
	str	r2, [r3, #0]
	b	.L592
.L591:
	ldr	r3, .L655+4
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bcs	.L592
	ldr	r3, .L655+4
	ldr	r2, [r3, #4]
	ldr	r3, .L655+24
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	mov	r2, r3, lsr #7
	ldr	r3, [fp, #-16]
	rsb	r3, r3, r2
	mov	r2, r3, asl #12
	ldr	r3, .L655+4
	ldr	r3, [r3, #4]
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, .L655+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bls	.L592
	ldr	r3, .L655+4
	ldr	r2, [fp, #-16]
	str	r2, [r3, #0]
.L592:
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+4
	ldrh	r1, [r2, #10]
	ldr	r2, .L655+68
	ldr	r0, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r0, r2
	ldrh	r2, [r2, #2]
	mov	r0, r2
	ldr	r2, .L655+4
	ldrh	r2, [r2, #10]
	rsb	r2, r2, r0
	mov	r0, r2
	ldr	r2, .L655+4
	ldr	r2, [r2, #0]
	rsb	ip, r2, #4096
	mul	r2, ip, r0
	mov	r2, r2, lsr #12
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	add	r2, r1, r2
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+4
	ldrh	r3, [r3, #8]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L655+68
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r3
	ldr	r3, .L655+4
	ldrh	r3, [r3, #8]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r0
	mov	r0, r3
	ldr	r3, .L655+4
	ldr	r3, [r3, #0]
	rsb	ip, r3, #4096
	mul	r3, ip, r0
	mov	r3, r3, lsr #12
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r1, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
.L585:
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L648
.L593:
	mov	r2, #1
	ldr	r3, [fp, #-36]
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L655+52
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	bne	.L584
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r2, r0
	ldr	r3, .L655+28
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcs	.L594
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+32
	mov	r3, #3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L595
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #4]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	b	.L584
.L595:
	mov	r3, #1
	str	r3, [fp, #-32]
	b	.L597
.L598:
	ldr	r2, [fp, #-32]
	ldr	r3, .L655+68
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L597:
	ldr	r3, [fp, #-32]
	cmp	r3, #9
	ble	.L598
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L599
.L600:
	ldr	r2, [fp, #-32]
	ldr	r3, .L655+36
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L599:
	ldr	r3, [fp, #-32]
	cmp	r3, #9
	ble	.L600
	b	.L584
.L594:
	mov	r2, #1
	ldr	r3, [fp, #-36]
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L655+52
	ldr	r3, [r3, #0]
	orr	r2, r2, r3
	ldr	r3, .L655+52
	str	r2, [r3, #0]
	b	.L584
.L647:
	mov	r0, r0	@ nop
	b	.L584
.L648:
	mov	r0, r0	@ nop
.L584:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L580:
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	ble	.L601
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L602
.L605:
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L603
	ldr	r3, [fp, #-36]
	str	r3, [fp, #-20]
	b	.L604
.L603:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L602:
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	blt	.L605
.L604:
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	bne	.L606
	ldr	r3, [fp, #-40]
	ldr	r3, [r3, #0]
	str	r3, [fp, #-112]
	ldr	r3, [fp, #-40]
	ldr	r2, [r3, #4]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, #64
	add	r2, r3, #127
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #7
	str	r3, [fp, #-76]
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L607
.L623:
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L608
	mov	r3, #1
	str	r3, [fp, #-32]
	b	.L609
.L610:
	ldr	r2, [fp, #-32]
	ldr	r3, .L655+68
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L609:
	ldr	r3, [fp, #-32]
	cmp	r3, #9
	ble	.L610
	b	.L611
.L608:
	mov	r2, #1
	ldr	r3, [fp, #-36]
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L655+52
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L649
.L612:
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r0, [fp, #-28]
	bl	Sqrt
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r2, [fp, #-112]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bge	.L613
	ldr	r3, [fp, #-112]
	ldr	r2, [fp, #-28]
	rsb	r3, r3, r2
	b	.L614
.L613:
	mov	r3, #0
.L614:
	str	r3, [fp, #-24]
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	beq	.L615
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L616
.L615:
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #4]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L617
.L656:
	.align	2
.L655:
	.word	shake_all_array
	.word	point_edge
	.word	edge_first
	.word	drv_num_nokey
	.word	sen_num_nokey
	.word	point_edge+8
	.word	edge_first_coe
	.word	shake_min
	.word	point_delay
	.word	point_report
	.word	65535
	.word	65534
	.word	point_decimal
	.word	point_shake
	.word	save_dr.4569
	.word	save_dn.4570
	.word	point_pointer
	.word	point_stretch
.L616:
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r5, [r3, #2]
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-24]
	mul	r3, r1, r2
	mov	r0, r3
	ldr	r1, [fp, #-28]
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r5, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r4, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r5, r3
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r2, r3, r2
	ldr	r1, [fp, #-24]
	mul	r3, r1, r2
	mov	r0, r3
	ldr	r1, [fp, #-28]
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r5, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, [r4, #0]
	mov	r2, r2, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r2
	mov	r3, r3, ror #20
	str	r3, [r4, #0]
.L617:
	ldr	r3, [fp, #-76]
	cmp	r3, #0
	ble	.L611
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bne	.L618
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	beq	.L618
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [fp, #-16]
	str	r2, [r3, #0]
	ldr	r1, [fp, #-36]
	ldr	r2, [fp, #-36]
	ldr	r0, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r0, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+68
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r0, [r3, #2]
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r0, .L655+48
	mov	r3, #2
	mov	r1, r1, asl #2
	add	r1, r0, r1
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	ldr	r0, [fp, #-36]
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, .L655+68
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, .L655+48
	mov	r3, r0, asl #2
	add	r3, r1, r3
	strh	r2, [r3, #0]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #4]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, asr #16
	ldr	r3, [fp, #-76]
	cmp	r2, r3
	ble	.L619
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r2, [r3, #2]
	ldr	r3, .L655+40
	cmp	r2, r3
	beq	.L619
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	add	r2, r2, #1
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
.L619:
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, asr #16
	ldr	r3, [fp, #-76]
	rsb	r3, r3, #0
	cmp	r2, r3
	bge	.L620
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	cmp	r3, #0
	beq	.L620
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	sub	r2, r2, #1
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
.L620:
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, asr #16
	ldr	r3, [fp, #-76]
	cmp	r2, r3
	ble	.L621
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L655+44
	cmp	r2, r3
	bgt	.L621
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r3, #1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
.L621:
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r2, r3, asr #16
	ldr	r3, [fp, #-76]
	rsb	r3, r3, #0
	cmp	r2, r3
	bge	.L650
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r3, #0
	ble	.L651
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	sub	r3, r3, #1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
	b	.L611
.L618:
	ldr	r2, [fp, #-36]
	ldr	r1, .L655+48
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
	ldr	r3, [fp, #-36]
	ldr	r2, .L655+48
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	strh	r2, [r3, #0]	@ movhi
	b	.L611
.L649:
	mov	r0, r0	@ nop
	b	.L611
.L650:
	mov	r0, r0	@ nop
	b	.L611
.L651:
	mov	r0, r0	@ nop
.L611:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L607:
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	ble	.L623
	b	.L646
.L606:
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	ble	.L652
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L626
.L628:
	ldr	r3, [fp, #-36]
	add	r1, r3, #1
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	mvn	r3, #99
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r1, r3, #1
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldr	r2, [r3, #4]
	mvn	r3, #63
	mov	r1, r1, asl #2
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L626:
	ldr	r3, [fp, #-36]
	cmp	r3, #7
	bgt	.L627
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	blt	.L628
.L627:
	ldr	r3, [fp, #-40]
	ldr	r3, [r3, #4]
	cmp	r3, #127
	bgt	.L629
	ldr	r3, [fp, #-40]
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-40]
	add	r3, r3, #8
	ldr	r3, [r3, #4]
	cmp	r2, r3
	bgt	.L630
.L629:
	ldr	r3, [fp, #-108]
	str	r3, [fp, #-112]
	ldr	r3, [fp, #-72]
	str	r3, [fp, #-76]
	b	.L631
.L630:
	ldr	r4, [fp, #-108]
	ldr	r3, [fp, #-40]
	ldr	r3, [r3, #4]
	rsb	r3, r3, #128
	ldr	r1, [fp, #-108]
	ldr	r2, [fp, #-104]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r3, [fp, #-40]
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-40]
	add	r3, r3, #8
	ldr	r3, [r3, #4]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r3, r4, r3
	str	r3, [fp, #-112]
	mov	r3, #128
	str	r3, [fp, #-76]
.L631:
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L632
.L645:
	ldr	r3, .L655+68
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L633
	mov	r3, #1
	str	r3, [fp, #-32]
	b	.L634
.L635:
	ldr	r2, [fp, #-32]
	ldr	r3, .L655+68
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L634:
	ldr	r3, [fp, #-32]
	cmp	r3, #9
	ble	.L635
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+56
	mov	r1, #128
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+60
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	b	.L636
.L633:
	mov	r2, #1
	ldr	r3, [fp, #-36]
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L655+52
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L653
.L637:
	ldr	r3, .L655+64
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r0, [fp, #-28]
	bl	Sqrt
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r2, [fp, #-112]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	ble	.L654
.L638:
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+60
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	ble	.L639
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+56
	ldr	r3, [r3, r2, asl #2]
	str	r3, [fp, #-24]
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+60
	ldr	r1, [fp, #-28]
	str	r1, [r3, r2, asl #2]
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r1, [r3, #2]
	ldr	r3, .L655+64
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r3, [r3, #2]
	mov	r0, r3
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #2]
	rsb	r0, r3, r0
	ldr	ip, [fp, #-24]
	mul	r3, ip, r0
	add	r0, r3, #127
	cmp	r3, #0
	movlt	r3, r0
	mov	r3, r3, asr #7
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r1, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r2, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L655+64
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r3
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r0, r3, r0
	ldr	ip, [fp, #-24]
	mul	r3, ip, r0
	add	r0, r3, #127
	cmp	r3, #0
	movlt	r3, r0
	mov	r3, r3, asr #7
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r1, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L636
.L639:
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L640
.L644:
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	beq	.L641
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	bne	.L642
.L641:
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L655+68
	ldr	r1, [r2, #4]
	ldr	r2, [fp, #-36]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L636
.L642:
	ldr	r2, [fp, #-32]
	mvn	r3, #99
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	ble	.L643
	ldr	r3, [fp, #-32]
	add	r2, r3, #1
	mvn	r3, #99
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bgt	.L643
	ldr	r3, [fp, #-32]
	add	r2, r3, #1
	mvn	r3, #63
	mov	r2, r2, asl #2
	sub	ip, fp, #12
	add	r2, ip, r2
	add	r3, r2, r3
	ldr	r4, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r2, r3, #1
	mvn	r3, #99
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-28]
	rsb	r3, r3, r2
	ldr	r1, [fp, #-32]
	mvn	r2, #63
	mov	r1, r1, asl #2
	sub	ip, fp, #12
	add	r1, ip, r1
	add	r2, r1, r2
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-32]
	add	r0, r2, #1
	mvn	r2, #63
	mov	r0, r0, asl #2
	sub	ip, fp, #12
	add	r0, ip, r0
	add	r2, r0, r2
	ldr	r2, [r2, #0]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r1, [fp, #-32]
	mvn	r3, #99
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-32]
	add	r0, r3, #1
	mvn	r3, #99
	mov	r0, r0, asl #2
	sub	ip, fp, #12
	add	r0, ip, r0
	add	r3, r0, r3
	ldr	r3, [r3, #0]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r3, r4, r3
	str	r3, [fp, #-24]
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+56
	ldr	r1, [fp, #-24]
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-36]
	ldr	r3, .L655+60
	ldr	r1, [fp, #-28]
	str	r1, [r3, r2, asl #2]
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r1, [r3, #2]
	ldr	r3, .L655+64
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r3, [r3, #2]
	mov	r0, r3
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #2]
	rsb	r0, r3, r0
	ldr	ip, [fp, #-24]
	mul	r3, ip, r0
	add	r3, r3, #64
	add	r0, r3, #127
	cmp	r3, #0
	movlt	r3, r0
	mov	r3, r3, asr #7
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r1, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r2, #2]	@ movhi
	ldr	r3, .L655+68
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L655+68
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L655+64
	ldr	r0, [r3, #0]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r3
	ldr	r3, .L655+68
	ldr	ip, [r3, #4]
	ldr	r3, [fp, #-36]
	mov	r3, r3, asl #2
	add	r3, ip, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r0, r3, r0
	ldr	ip, [fp, #-24]
	mul	r3, ip, r0
	add	r3, r3, #64
	add	r0, r3, #127
	cmp	r3, #0
	movlt	r3, r0
	mov	r3, r3, asr #7
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r1, r3
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L636
.L643:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L640:
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	ble	.L644
	b	.L636
.L653:
	mov	r0, r0	@ nop
	b	.L636
.L654:
	mov	r0, r0	@ nop
.L636:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L632:
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	ble	.L645
	b	.L646
.L652:
	mov	r0, r0	@ nop
.L646:
	sub	sp, fp, #12
	ldmfd	sp!, {r4, r5, fp, lr}
	bx	lr
	.size	PointStretch, .-PointStretch
	.align	2
	.type	ResetMask, %function
ResetMask:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	ldr	r3, .L676
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L658
	ldr	r3, .L676
	mov	r2, #0
	str	r2, [r3, #0]
.L658:
	ldr	r3, .L676+4
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #64
	cmp	r3, #0
	bne	.L671
.L659:
	ldr	r3, .L676+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L672
	ldr	r3, .L676+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L673
.L662:
	ldr	r3, .L676+16
	ldr	r3, [r3, #0]
	cmn	r3, #15
	bne	.L663
	ldr	r3, .L676+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L674
	ldr	r3, .L676+16
	mov	r2, #-268435455
	str	r2, [r3, #0]
	b	.L670
.L663:
	ldr	r3, .L676+16
	ldr	r3, [r3, #0]
	cmp	r3, #-268435456
	bls	.L665
	ldr	r3, .L676+16
	ldr	r3, [r3, #0]
	sub	r2, r3, #1
	ldr	r3, .L676+16
	str	r2, [r3, #0]
	ldr	r3, .L676+16
	ldr	r3, [r3, #0]
	cmp	r3, #-268435456
	bne	.L675
	ldr	r3, .L676+12
	ldr	r2, [r3, #0]
	ldr	r3, .L676
	str	r2, [r3, #0]
	ldr	r2, .L676+4
	ldr	r3, [r2, #0]
	orr	r3, r3, #1073741824
	str	r3, [r2, #0]
	b	.L670
.L665:
	ldr	r3, .L676+20
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bgt	.L667
	ldr	r3, .L676+24
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L668
.L667:
	ldr	r3, .L676+28
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L676+16
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L676+28
	mov	r2, #0
	str	r2, [r3, #0]
	b	.L670
.L668:
	ldr	r3, .L676+28
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	ldr	r3, .L676+28
	str	r2, [r3, #0]
	ldr	r3, .L676+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L669
	ldr	r3, .L676+24
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #0]
	ldr	r3, .L676+16
	str	r2, [r3, #0]
	b	.L670
.L669:
	ldr	r2, .L676+16
	ldr	r3, .L676+24
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r2, r0
	ldr	r3, .L676+8
	ldr	r3, [r3, #0]
	bic	r3, r3, #-16777216
	cmp	r2, r3
	bls	.L670
	ldr	r3, .L676+8
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #24
	ldr	r3, .L676+28
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcs	.L670
	ldr	r3, .L676+16
	mvn	r2, #14
	str	r2, [r3, #0]
	b	.L670
.L671:
	mov	r0, r0	@ nop
	b	.L670
.L672:
	mov	r0, r0	@ nop
	b	.L670
.L673:
	mov	r0, r0	@ nop
	b	.L670
.L674:
	mov	r0, r0	@ nop
	b	.L670
.L675:
	mov	r0, r0	@ nop
.L670:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L677:
	.align	2
.L676:
	.word	reset_mask_send
	.word	global_state
	.word	reset_mask_dis
	.word	reset_mask_type
	.word	reset_mask_max
	.word	point_num
	.word	point_pointer
	.word	reset_mask_count
	.size	ResetMask, .-ResetMask
	.align	2
	.type	ConfigCoorMulti, %function
ConfigCoorMulti:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #28
	str	r0, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L679
.L685:
	ldr	r3, [fp, #-16]
	add	r3, r3, #247
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L680
	ldr	r3, [fp, #-16]
	add	r3, r3, #247
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #63
	cmp	r3, #0
	bne	.L681
	ldr	r3, [fp, #-16]
	add	r3, r3, #247
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asr #16
	cmp	r3, #3
	bgt	.L681
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
	b	.L680
.L681:
	mov	r3, #0
	b	.L682
.L680:
	ldr	r3, [fp, #-16]
	add	r3, r3, #251
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L683
	ldr	r3, [fp, #-16]
	add	r3, r3, #251
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #63
	cmp	r3, #0
	bne	.L684
	ldr	r3, [fp, #-16]
	add	r3, r3, #251
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asr #16
	cmp	r3, #3
	bgt	.L684
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
	b	.L683
.L684:
	mov	r3, #0
	b	.L682
.L683:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L679:
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	ble	.L685
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	beq	.L686
	ldr	r3, [fp, #-8]
	cmp	r3, #4
	ble	.L687
.L686:
	mov	r3, #0
	b	.L682
.L687:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L688
.L693:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L689
.L692:
	ldr	r3, [fp, #-12]
	add	r3, r3, #4
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #63
	ble	.L690
	mov	r3, #0
	b	.L682
.L690:
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L691
	ldr	r3, [fp, #-12]
	add	r3, r3, #4
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #4
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-16]
	add	r3, r1, r3
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-24]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bge	.L691
	mov	r3, #0
	b	.L682
.L691:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L689:
	ldr	r3, [fp, #-16]
	cmp	r3, #63
	ble	.L692
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L688:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	blt	.L693
	mov	r3, #1
.L682:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	ConfigCoorMulti, .-ConfigCoorMulti
	.align	2
	.type	ConfigFilter, %function
ConfigFilter:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 80
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #84
	str	r0, [fp, #-80]
	mov	r3, #0
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-80]
	add	r3, r3, #968
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bls	.L696
	ldr	r3, [fp, #-80]
	add	r3, r3, #1020
	ldr	r3, [r3, #0]
	cmp	r3, #256
	bhi	.L696
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L697
.L700:
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-12]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r3, r3, #243
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-80]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r2, [fp, #-12]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	mvn	r3, #39
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-12]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r3, r3, #245
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-80]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r2, [fp, #-12]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	mvn	r3, #71
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #127
	bls	.L698
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3, asl #24
	mvn	r3, r3, lsr #24
	mvn	r2, #39
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r2, r1, r2
	str	r3, [r2, #0]
.L698:
	ldr	r2, [fp, #-12]
	mvn	r3, #71
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #127
	bls	.L699
	ldr	r1, [fp, #-12]
	ldr	r2, [fp, #-12]
	mvn	r3, #71
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mvn	r3, r3, asl #24
	mvn	r3, r3, lsr #24
	mvn	r2, #71
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r2, r1, r2
	str	r3, [r2, #0]
.L699:
	ldr	r2, [fp, #-12]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-12]
	mvn	r3, #71
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L697:
	ldr	r3, [fp, #-12]
	cmp	r3, #7
	ble	.L700
	ldr	r3, [fp, #-80]
	add	r3, r3, #968
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	beq	.L701
	ldr	r3, [fp, #-80]
	add	r3, r3, #968
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	cmp	r3, #0
	bne	.L696
.L701:
	mov	r3, #1
	b	.L702
.L696:
	mov	r3, #0
.L702:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	ConfigFilter, .-ConfigFilter
	.align	2
	.type	ConfigKeyMap, %function
ConfigKeyMap:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r3, r3, #868
	ldr	r3, [r3, #0]
	cmp	r3, #1
	beq	.L705
	mov	r3, #0
	b	.L706
.L705:
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L707
.L711:
	ldr	r3, [fp, #-16]
	add	r3, r3, #880
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L708
	mov	r3, #0
	b	.L706
.L708:
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, #872
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r1, r3, asr #16
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, #872
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r1, r3
	ble	.L709
	mov	r3, #0
	b	.L706
.L709:
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, #876
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r1, r3, asr #16
	ldr	r2, [fp, #-8]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, #876
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r1, r3
	ble	.L710
	mov	r3, #0
	b	.L706
.L710:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L707:
	ldr	r3, [fp, #-8]
	cmp	r3, #7
	ble	.L711
	mov	r3, #1
.L706:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	ConfigKeyMap, .-ConfigKeyMap
	.align	2
	.type	DiagonalDistance, %function
DiagonalDistance:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L718
	ldr	r3, [r3, #0]
	ldr	r2, .L718
	ldr	r2, [r2, #0]
	mov	r1, r2
	mul	r2, r1, r3
	ldr	r3, .L718+4
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L718+4
	ldr	r3, [r3, #0]
	mov	r0, r3
	mul	r3, r0, r1
	add	r3, r2, r3
	add	r2, r3, #15
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #4
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bne	.L714
	mov	r3, #1
	str	r3, [fp, #-12]
.L714:
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bne	.L715
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #2]
	ldr	r2, .L718
	ldr	r2, [r2, #0]
	mov	r1, r2
	mul	r2, r1, r3
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L718+4
	ldr	r3, [r3, #0]
	mov	r0, r3
	mul	r3, r0, r1
	rsb	r3, r3, r2
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	str	r3, [fp, #-8]
	b	.L716
.L715:
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #2]
	ldr	r2, .L718
	ldr	r2, [r2, #0]
	mov	r1, r2
	mul	r2, r1, r3
	ldr	r3, [fp, #-16]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L718+4
	ldr	r3, [r3, #0]
	mov	r0, r3
	mul	r3, r0, r1
	add	r2, r2, r3
	ldr	r3, .L718
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L718+4
	ldr	r3, [r3, #0]
	mov	r0, r3
	mul	r3, r0, r1
	mov	r3, r3, asl #6
	rsb	r3, r3, #0
	add	r3, r2, r3
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	str	r3, [fp, #-8]
.L716:
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	mul	r3, r1, r2
	mov	r0, r3
	ldr	r1, [fp, #-12]
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L719:
	.align	2
.L718:
	.word	sen_num_nokey
	.word	drv_num_nokey
	.size	DiagonalDistance, .-DiagonalDistance
	.align	2
	.type	DiagonalCompress, %function
DiagonalCompress:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #40
	str	r0, [fp, #-32]
	str	r1, [fp, #-36]
	str	r2, [fp, #-40]
	str	r3, [fp, #-44]
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	beq	.L733
.L721:
	ldr	r2, [fp, #-40]
	ldr	r3, [fp, #-44]
	cmp	r2, r3
	ble	.L723
	ldr	r2, [fp, #-40]
	ldr	r3, [fp, #-44]
	rsb	r3, r3, r2
	mov	r1, r3, asl #7
	ldr	r2, [fp, #-44]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r0, r1
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	add	r3, r3, #128
	str	r3, [fp, #-8]
	b	.L724
.L723:
	mov	r3, #128
	str	r3, [fp, #-8]
.L724:
	ldr	r3, [fp, #-8]
	cmp	r3, #256
	ble	.L725
	mov	r3, #256
	str	r3, [fp, #-8]
.L725:
	ldr	r3, [fp, #-32]
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-32]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	beq	.L726
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	str	r3, [fp, #-20]
.L726:
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-24]
	mul	r3, r1, r2
	str	r3, [fp, #-24]
	ldr	r3, .L734+4
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r2, [fp, #-20]
	mul	r3, r1, r2
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-20]
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	add	r2, r2, r3
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-12]
	rsb	r1, r3, r1
	ldr	r0, [fp, #-8]
	mul	r3, r0, r1
	add	r1, r3, #255
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #8
	add	r3, r2, r3
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-24]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-12]
	add	r2, r2, r3
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-16]
	rsb	r1, r3, r1
	ldr	r0, [fp, #-8]
	mul	r3, r0, r1
	add	r1, r3, #255
	cmp	r3, #0
	movlt	r3, r1
	mov	r3, r3, asr #8
	add	r3, r2, r3
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-20]
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	ldr	r0, [fp, #-24]
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, .L734+4
	ldr	r3, [r3, #0]
	ldr	r0, [fp, #-20]
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	beq	.L727
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	str	r3, [fp, #-20]
.L727:
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bgt	.L728
	mov	r3, #1
	str	r3, [fp, #-24]
.L728:
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bgt	.L729
	mov	r3, #1
	str	r3, [fp, #-20]
.L729:
	ldr	r3, .L734+4
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bgt	.L730
	ldr	r3, .L734+4
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-24]
.L730:
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L731
	ldr	r3, .L734
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	sub	r3, r3, #1
	str	r3, [fp, #-20]
.L731:
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, [fp, #-32]
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r2, [fp, #-32]
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	b	.L732
.L733:
	mov	r0, r0	@ nop
.L732:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L735:
	.align	2
.L734:
	.word	sen_num_nokey
	.word	drv_num_nokey
	.size	DiagonalCompress, .-DiagonalCompress
	.align	2
	.type	PointDiagonal, %function
PointDiagonal:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	ldr	r3, .L754
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L753
.L737:
	ldr	r3, .L754
	ldr	r2, [r3, #0]
	ldr	r3, .L754
	ldr	r1, [r3, #0]
	mul	r3, r1, r2
	str	r3, [fp, #-16]
	ldr	r3, .L754
	ldr	r2, [r3, #0]
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r2
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-8]
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L739
.L751:
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L740
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L741
.L740:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #3
	mov	r3, r2, asl r3
	mvn	r3, r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
	b	.L742
.L741:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #3
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	bne	.L743
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bhi	.L744
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bls	.L745
.L744:
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L754+12
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r1
	cmp	r2, r3
	bcc	.L746
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L754+16
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r1
	cmp	r2, r3
	bcc	.L746
.L745:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #2
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	orr	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
	b	.L743
.L746:
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bhi	.L747
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L754+16
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r1
	cmp	r2, r3
	bcs	.L748
.L747:
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L754+12
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-8]
	rsb	r3, r3, r1
	cmp	r2, r3
	bcc	.L749
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bhi	.L749
.L748:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #3
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	orr	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
	b	.L743
.L749:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #1
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	orr	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
.L743:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #2
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L742
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r1, #1
	mov	r3, r1, asl r3
	mov	r1, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	DiagonalDistance
	mov	r3, r0
	str	r3, [fp, #-12]
	ldr	r3, [fp, #-16]
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	blt	.L750
	ldr	r3, .L754+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r1, #1
	mov	r3, r1, asl r3
	mov	r1, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-16]
	bl	DiagonalCompress
	b	.L742
.L750:
	ldr	r3, [fp, #-16]
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L742
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #3
	mov	r3, r2, asl r3
	mvn	r3, r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	and	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #1
	mov	r2, #1
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L754+8
	ldr	r3, [r3, #0]
	orr	r2, r2, r3
	ldr	r3, .L754+8
	str	r2, [r3, #0]
.L742:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L739:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L751
	b	.L752
.L753:
	mov	r0, r0	@ nop
.L752:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L755:
	.align	2
.L754:
	.word	diagonal
	.word	point_stretch
	.word	point_corner
	.word	drv_num_nokey
	.word	sen_num_nokey
	.size	PointDiagonal, .-PointDiagonal
	.align	2
	.type	PointSlope, %function
PointSlope:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, [fp, #-20]
	ldr	r3, .L761
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-20]
	add	r1, r3, #1
	ldr	r3, .L761
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r2
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-12]
	ldr	r1, [fp, #-12]
	mul	r3, r1, r2
	str	r3, [fp, #-12]
	ldr	r2, [fp, #-20]
	ldr	r3, .L761
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-20]
	add	r1, r3, #1
	ldr	r3, .L761
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	mul	r3, r1, r2
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	cmp	r3, #0
	bne	.L757
	mvn	r3, #0
	b	.L758
.L757:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	ble	.L759
	ldr	r3, [fp, #-12]
	mov	r2, r3, asl #10
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-8]
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	b	.L758
.L759:
	ldr	r3, [fp, #-8]
	mov	r2, r3, asl #10
	ldr	r1, [fp, #-12]
	ldr	r3, [fp, #-8]
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
.L758:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L762:
	.align	2
.L761:
	.word	point_report
	.size	PointSlope, .-PointSlope
	.align	2
	.type	PointExtend, %function
PointExtend:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 32
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #36
	mov	r3, #5
	str	r3, [fp, #-16]
	ldr	r3, .L787
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L781
.L764:
	mov	r3, #0
	str	r3, [fp, #-40]
	b	.L766
.L779:
	ldr	r3, .L787+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L782
.L767:
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L769
.L772:
	ldr	r2, [fp, #-36]
	ldr	r3, .L787+4
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L783
.L770:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L769:
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L772
	b	.L771
.L783:
	mov	r0, r0	@ nop
.L771:
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L784
.L773:
	ldr	r3, .L787+4
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, .L787+4
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	bl	PointDistance
	mov	r3, r0
	cmp	r3, #255
	bls	.L785
.L774:
	ldr	r0, [fp, #-40]
	mov	r1, #1
	bl	PointSlope
	mov	r3, r0
	str	r3, [fp, #-24]
	mov	r3, #2
	str	r3, [fp, #-36]
	b	.L775
.L777:
	ldr	r0, [fp, #-40]
	ldr	r1, [fp, #-36]
	bl	PointSlope
	mov	r3, r0
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	blt	.L776
	ldr	r3, .L787
	ldr	r3, [r3, #0]
	rsb	r2, r3, #128
	ldr	r1, [fp, #-24]
	mul	r3, r1, r2
	add	r2, r3, #127
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #7
	mov	r2, r3
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L776
	ldr	r3, .L787
	ldr	r3, [r3, #0]
	add	r2, r3, #128
	ldr	r1, [fp, #-24]
	mul	r3, r1, r2
	add	r2, r3, #127
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #7
	mov	r2, r3
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	blt	.L776
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L775:
	ldr	r3, [fp, #-16]
	sub	r2, r3, #1
	ldr	r3, [fp, #-36]
	cmp	r2, r3
	bgt	.L777
.L776:
	ldr	r3, [fp, #-16]
	sub	r2, r3, #1
	ldr	r3, [fp, #-36]
	cmp	r2, r3
	bgt	.L786
.L778:
	ldr	r3, .L787+4
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r2, r3, r2
	ldr	r3, .L787+4
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	mov	r3, r3, asl #1
	rsb	r3, r3, #0
	add	r3, r2, r3
	str	r3, [fp, #-32]
	ldr	r3, .L787+4
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	mov	r3, r2
	mov	r3, r3, asl #1
	add	r2, r3, r2
	ldr	r3, .L787+4
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #1
	rsb	r3, r3, #0
	add	r3, r2, r3
	str	r3, [fp, #-28]
	ldr	r3, .L787+4
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r3, .L787+4
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, .L787+4
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-40]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r0, [fp, #-32]
	ldr	r1, [fp, #-28]
	bl	PointRange
	mov	r3, r0
	str	r3, [r4, #0]
	b	.L768
.L782:
	mov	r0, r0	@ nop
	b	.L768
.L784:
	mov	r0, r0	@ nop
	b	.L768
.L785:
	mov	r0, r0	@ nop
	b	.L768
.L786:
	mov	r0, r0	@ nop
.L768:
	ldr	r3, [fp, #-40]
	add	r3, r3, #1
	str	r3, [fp, #-40]
.L766:
	ldr	r3, [fp, #-40]
	cmp	r3, #9
	ble	.L779
	b	.L780
.L781:
	mov	r0, r0	@ nop
.L780:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L788:
	.align	2
.L787:
	.word	point_extend
	.word	point_report
	.size	PointExtend, .-PointExtend
	.align	2
	.type	PressureSave, %function
PressureSave:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	ldr	r3, .L796
	ldr	r3, [r3, #0]
	and	r3, r3, #4096
	cmp	r3, #0
	beq	.L795
.L790:
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L792
.L793:
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	ldr	r3, .L796+4
	ldr	r3, [r3, r1, asl #2]
	mov	r1, r3, lsr #28
	ldr	r3, .L796+8
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	ldr	r3, .L796+4
	ldr	r3, [r3, r1, asl #2]
	bic	r1, r3, #-268435456
	ldr	r3, .L796+4
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L792:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L793
	b	.L794
.L795:
	mov	r0, r0	@ nop
.L794:
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L797:
	.align	2
.L796:
	.word	point_num
	.word	point_now
	.word	pressure_now
	.size	PressureSave, .-PressureSave
	.align	2
	.type	PointPressure, %function
PointPressure:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L799
.L807:
	ldr	r3, .L809
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L800
	ldr	r3, .L809
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L800
	ldr	r1, [fp, #-12]
	ldr	r3, .L809
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r2, r3, r2
	ldr	r3, .L809+4
	str	r2, [r3, r1, asl #2]
	mov	r3, #1
	str	r3, [fp, #-8]
	b	.L801
.L802:
	ldr	r2, [fp, #-8]
	ldr	r3, .L809
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, .L809
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-12]
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L801:
	ldr	r3, [fp, #-8]
	cmp	r3, #7
	ble	.L802
	b	.L803
.L800:
	ldr	r2, [fp, #-12]
	ldr	r3, .L809+4
	ldr	r3, [r3, r2, asl #2]
	add	r3, r3, #1
	mov	r2, r3, lsr #1
	ldr	r3, .L809
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	add	r2, r2, r3
	ldr	r3, .L809
	ldr	r1, [r3, #4]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	add	r2, r2, r3
	ldr	r3, .L809
	ldr	r1, [r3, #8]
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	mov	r3, r3, lsr #1
	add	r2, r2, r3
	ldr	r1, [fp, #-12]
	ldr	r3, .L809+4
	ldr	r3, [r3, r1, asl #2]
	rsb	r3, r3, r2
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-8]
	cmp	r3, #1
	ble	.L804
	ldr	r3, [fp, #-8]
	sub	r3, r3, #2
	str	r3, [fp, #-8]
	b	.L805
.L804:
	ldr	r3, [fp, #-8]
	cmn	r3, #1
	bge	.L806
	ldr	r3, [fp, #-8]
	add	r3, r3, #2
	str	r3, [fp, #-8]
	b	.L805
.L806:
	mov	r3, #0
	str	r3, [fp, #-8]
.L805:
	ldr	r2, [fp, #-12]
	ldr	r1, [fp, #-12]
	ldr	r3, .L809+4
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, [fp, #-8]
	add	r1, r1, r3
	ldr	r3, .L809+4
	str	r1, [r3, r2, asl #2]
.L803:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L799:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L807
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L810:
	.align	2
.L809:
	.word	pressure_pointer
	.word	pressure_report
	.size	PointPressure, .-PointPressure
	.align	2
	.global	gsl_ReportPressure
	.type	gsl_ReportPressure, %function
gsl_ReportPressure:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L812
.L819:
	ldr	r3, .L821
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-8]
	cmp	r2, r3
	bge	.L813
	ldr	r2, [fp, #-8]
	ldr	r3, .L821+4
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	bne	.L814
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	b	.L818
.L814:
	ldr	r2, [fp, #-8]
	ldr	r3, .L821+4
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #7
	bhi	.L816
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	mov	r2, #1
	str	r2, [r3, #0]
	b	.L818
.L816:
	ldr	r2, [fp, #-8]
	ldr	r3, .L821+4
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #70
	bls	.L817
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	mov	r2, #63
	str	r2, [r3, #0]
	b	.L818
.L817:
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	ldr	r1, [fp, #-8]
	ldr	r2, .L821+4
	ldr	r2, [r2, r1, asl #2]
	sub	r2, r2, #7
	str	r2, [r3, #0]
	b	.L818
.L813:
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-16]
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
.L818:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L812:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L819
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L822:
	.align	2
.L821:
	.word	point_num
	.word	pressure_now
	.size	gsl_ReportPressure, .-gsl_ReportPressure
	.align	2
	.global	gsl_TouchNear
	.type	gsl_TouchNear, %function
gsl_TouchNear:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	mov	r3, #0
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	gsl_TouchNear, .-gsl_TouchNear
	.align	2
	.type	gsl_id_reg_init, %function
gsl_id_reg_init:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L826
.L829:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L827
.L828:
	ldr	r2, [fp, #-8]
	ldr	r0, [fp, #-12]
	ldr	r1, .L838
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	mov	r2, #0
	str	r2, [r1, r3, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L827:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L828
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L826:
	ldr	r3, [fp, #-8]
	cmp	r3, #29
	ble	.L829
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L830
.L833:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L831
.L832:
	ldr	r2, [fp, #-8]
	ldr	r0, [fp, #-12]
	ldr	r1, .L838+4
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	add	r3, r3, r0
	mov	r2, #0
	str	r2, [r1, r3, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L831:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L832
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L830:
	ldr	r3, [fp, #-8]
	cmp	r3, #7
	ble	.L833
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L834
.L835:
	ldr	r2, [fp, #-12]
	ldr	r3, .L838+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-12]
	ldr	r3, .L838+12
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-12]
	ldr	r3, .L838+16
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L834:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L835
	ldr	r3, .L838+20
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+24
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L836
	ldr	r3, .L838+28
	mov	r2, #0
	str	r2, [r3, #0]
.L836:
	ldr	r3, .L838+32
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+36
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+40
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+44
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+48
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+52
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+56
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+60
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+64
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+68
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+72
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L838+76
	mov	r2, #0
	str	r2, [r3, #0]
	bl	GestureInit
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L839:
	.align	2
.L838:
	.word	point_array
	.word	pressure_array
	.word	point_delay
	.word	filter_deep
	.word	point_decimal
	.word	point_edge
	.word	point_n
	.word	point_num
	.word	prev_num
	.word	point_shake
	.word	reset_mask_send
	.word	reset_mask_max
	.word	reset_mask_count
	.word	point_near
	.word	point_corner
	.word	global_state
	.word	inte_count
	.word	csensor_count
	.word	double_click
	.word	gesture_last
	.size	gsl_id_reg_init, .-gsl_id_reg_init
	.align	2
	.type	DataCheck, %function
DataCheck:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	ldr	r3, .L847
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L841
	ldr	r3, .L847+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L841
	ldr	r3, .L847+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L841
	ldr	r3, .L847+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L842
.L841:
	mov	r3, #0
	b	.L843
.L842:
	ldr	r3, .L847+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L844
	ldr	r3, .L847+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L845
.L844:
	mov	r3, #0
	b	.L843
.L845:
	mov	r3, #1
.L843:
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L848:
	.align	2
.L847:
	.word	drv_num
	.word	drv_num_nokey
	.word	sen_num
	.word	sen_num_nokey
	.word	screen_x_max
	.word	screen_y_max
	.size	DataCheck, .-DataCheck
	.align	2
	.global	gsl_DataInit
	.type	gsl_DataInit, %function
gsl_DataInit:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #28
	str	r0, [fp, #-32]
	mov	r0, #1
	bl	gsl_id_reg_init
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L850
.L851:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L850:
	ldr	r3, [fp, #-24]
	cmp	r3, #9
	ble	.L851
	ldr	r3, .L942+4
	str	r3, [fp, #-20]
	ldr	r3, .L942+8
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L852
.L853:
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+176
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+180
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L852:
	ldr	r3, [fp, #-24]
	cmp	r3, #31
	ble	.L853
	ldr	r3, .L942+12
	mov	r2, #8
	str	r2, [r3, #0]
	ldr	r3, .L942+16
	mov	r2, #16384
	str	r2, [r3, #0]
	ldr	r3, .L942+20
	mov	r2, #4096
	str	r2, [r3, #0]
	ldr	r3, .L942+208
	mov	r2, #4
	str	r2, [r3, #0]
	ldr	r3, .L942+24
	mov	r2, #3
	str	r2, [r3, #0]
	ldr	r3, .L942+28
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+144
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+132
	ldr	r2, .L942+32
	str	r2, [r3, #0]
	ldr	r3, .L942+148
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L854
.L855:
	ldr	r3, [fp, #-24]
	ldr	r2, .L942+196
	mov	r1, #0
	strb	r1, [r2, r3]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L854:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L855
	ldr	r3, .L942+36
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L856
.L857:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+220
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+216
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+228
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+224
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L856:
	ldr	r3, [fp, #-24]
	cmp	r3, #1
	ble	.L857
	ldr	r3, .L942+220
	mov	r2, #4096
	str	r2, [r3, #0]
	ldr	r3, .L942+216
	mov	r2, #4096
	str	r2, [r3, #0]
	ldr	r3, .L942+40
	mov	r2, #480
	str	r2, [r3, #0]
	ldr	r3, .L942+44
	mov	r2, #800
	str	r2, [r3, #0]
	ldr	r3, .L942+48
	mov	r2, #10
	str	r2, [r3, #0]
	ldr	r3, .L942+52
	mov	r2, #16
	str	r2, [r3, #0]
	ldr	r3, .L942+56
	mov	r2, #10
	str	r2, [r3, #0]
	ldr	r3, .L942+60
	mov	r2, #16
	str	r2, [r3, #0]
	ldr	r3, .L942+64
	mov	r2, #10
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L858
.L859:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+68
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L858:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L859
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L860
.L861:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+72
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L860:
	ldr	r3, [fp, #-24]
	cmp	r3, #31
	ble	.L861
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L862
.L863:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+212
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L862:
	ldr	r3, [fp, #-24]
	cmp	r3, #15
	ble	.L863
	ldr	r3, .L942+76
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+80
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+84
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+88
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+92
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+156
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L864
.L865:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+160
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L864:
	ldr	r3, [fp, #-24]
	cmp	r3, #23
	ble	.L865
	ldr	r3, .L942+188
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+192
	ldr	r2, .L942+96
	str	r2, [r3, #0]
	ldr	r3, .L942+192
	ldr	r2, .L942+100
	str	r2, [r3, #4]
	ldr	r3, .L942+192
	ldr	r2, .L942+104
	str	r2, [r3, #8]
	ldr	r3, .L942+192
	ldr	r2, .L942+108
	str	r2, [r3, #12]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L866
.L867:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+168
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+172
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L866:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L867
	ldr	r3, .L942+112
	mov	r2, #32
	str	r2, [r3, #0]
	ldr	r3, .L942+112
	mov	r2, #96
	str	r2, [r3, #4]
	ldr	r3, .L942+136
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+140
	mov	r2, #128
	str	r2, [r3, #0]
	ldr	r3, .L942+116
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	beq	.L941
.L868:
	ldr	r3, [fp, #-32]
	ldr	r2, [r3, #0]
	ldr	r3, .L942+120
	cmp	r2, r3
	bhi	.L870
	ldr	r3, [fp, #-32]
	mov	r0, r3
	bl	ConfigCoorMulti
	mov	r3, r0
	cmp	r3, #0
	beq	.L871
	mov	r3, #512
	str	r3, [fp, #-16]
	b	.L875
.L871:
	ldr	r0, [fp, #-32]
	bl	ConfigFilter
	mov	r3, r0
	cmp	r3, #0
	beq	.L873
	mov	r3, #256
	str	r3, [fp, #-16]
	b	.L875
.L873:
	ldr	r3, [fp, #-32]
	mov	r0, r3
	bl	ConfigKeyMap
	mov	r3, r0
	cmp	r3, #0
	beq	.L874
	mov	r3, #241
	str	r3, [fp, #-16]
	b	.L875
.L874:
	mov	r3, #215
	str	r3, [fp, #-16]
	b	.L875
.L870:
	ldr	r3, [fp, #-32]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	cmp	r3, #512
	bhi	.L876
	ldr	r3, [fp, #-32]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	str	r3, [fp, #-16]
	b	.L875
.L876:
	mov	r3, #512
	str	r3, [fp, #-16]
.L875:
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L877
.L878:
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r2, [fp, #-24]
	mov	r2, r2, asl #2
	ldr	r1, [fp, #-32]
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L877:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L878
	b	.L879
.L880:
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L879:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+236
	cmp	r2, r3
	ble	.L880
	ldr	r3, [fp, #-32]
	ldr	r2, [r3, #0]
	ldr	r3, .L942+120
	cmp	r2, r3
	bhi	.L881
	ldr	r3, [fp, #-20]
	ldr	r2, [r3, #0]
	ldr	r3, .L942+8
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #4
	ldr	r2, [r3, #0]
	ldr	r3, .L942+52
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #8
	ldr	r2, [r3, #0]
	ldr	r3, .L942+56
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #12
	ldr	r2, [r3, #0]
	ldr	r3, .L942+60
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #16
	ldr	r2, [r3, #0]
	ldr	r3, .L942+64
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #20
	ldr	r2, [r3, #0]
	ldr	r3, .L942+12
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #24
	ldr	r2, [r3, #0]
	ldr	r3, .L942+16
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #28
	ldr	r2, [r3, #0]
	ldr	r3, .L942+20
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #32
	ldr	r2, [r3, #0]
	ldr	r3, .L942+208
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #36
	ldr	r2, [r3, #0]
	ldr	r3, .L942+24
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #52
	ldr	r2, [r3, #0]
	ldr	r3, .L942+28
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #56
	ldr	r2, [r3, #0]
	ldr	r3, .L942+36
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #60
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+40
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #64
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+44
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #68
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+48
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #72
	ldr	r2, [r3, #0]
	ldr	r3, .L942+124
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L882
.L883:
	ldr	r3, [fp, #-24]
	ldr	r2, [fp, #-24]
	add	r2, r2, #19
	mov	r2, r2, asl #2
	ldr	r1, [fp, #-20]
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	and	r1, r2, #255
	ldr	r2, .L942+196
	strb	r1, [r2, r3]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L882:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L883
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L884
.L885:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #23
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+220
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #25
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+216
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #27
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+228
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #29
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+224
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L884:
	ldr	r3, [fp, #-24]
	cmp	r3, #1
	ble	.L885
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L886
.L887:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #31
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+176
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #95
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+180
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L886:
	ldr	r3, [fp, #-24]
	cmp	r3, #63
	ble	.L887
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L888
.L889:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #159
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+68
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L888:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L889
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L890
.L891:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #163
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+72
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L890:
	ldr	r3, [fp, #-24]
	cmp	r3, #31
	ble	.L891
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L892
.L893:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #195
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+212
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L892:
	ldr	r3, [fp, #-24]
	cmp	r3, #15
	ble	.L893
	ldr	r3, [fp, #-20]
	add	r3, r3, #852
	ldr	r2, [r3, #0]
	ldr	r3, .L942+76
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #856
	ldr	r2, [r3, #0]
	ldr	r3, .L942+80
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #864
	ldr	r2, [r3, #0]
	ldr	r3, .L942+84
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #868
	ldr	r2, [r3, #0]
	ldr	r3, .L942+156
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L894
.L895:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #218
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+160
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L894:
	ldr	r3, [fp, #-24]
	cmp	r3, #23
	ble	.L895
	ldr	r3, [fp, #-20]
	add	r3, r3, #968
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+188
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L896
.L897:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #243
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+192
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L896:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L897
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L898
.L899:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #247
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+168
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L898:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L899
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L900
.L901:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #251
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+172
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L900:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L901
	ldr	r3, [fp, #-20]
	add	r3, r3, #1020
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+88
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L902
.L903:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+184
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L902:
	ldr	r3, [fp, #-24]
	cmp	r3, #255
	ble	.L903
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L904
.L905:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #448
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+200
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #480
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+204
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L904:
	ldr	r3, [fp, #-24]
	cmp	r3, #31
	ble	.L905
	ldr	r3, .L942+128
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L942+128
	mov	r2, #0
	str	r2, [r3, #4]
	b	.L906
.L881:
	ldr	r3, [fp, #-20]
	add	r3, r3, #64
	ldr	r2, [r3, #0]
	ldr	r3, .L942+124
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #68
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+48
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #72
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+52
	str	r3, [r2, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #72
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #16
	ldr	r3, .L942+56
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #76
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+60
	str	r3, [r2, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #76
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #16
	ldr	r3, .L942+64
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #80
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+44
	str	r3, [r2, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #80
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+40
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #84
	ldr	r2, [r3, #0]
	ldr	r3, .L942+208
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #88
	ldr	r2, [r3, #0]
	ldr	r3, .L942+76
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #92
	ldr	r2, [r3, #0]
	ldr	r3, .L942+80
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #96
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+112
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #96
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+112
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #100
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+128
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #100
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+128
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #104
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+88
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #108
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+92
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #112
	ldr	r2, [r3, #0]
	ldr	r3, .L942+84
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #124
	ldr	r2, [r3, #0]
	ldr	r3, .L942+116
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #128
	ldr	r2, [r3, #0]
	ldr	r3, .L942+12
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #132
	ldr	r2, [r3, #0]
	ldr	r3, .L942+16
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #136
	ldr	r2, [r3, #0]
	ldr	r3, .L942+20
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #140
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+220
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #140
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+220
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #144
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+216
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #144
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+216
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #148
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+228
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #148
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+228
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #152
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L942+224
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #152
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+224
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #156
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #24
	ldr	r3, .L942+68
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #156
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	and	r2, r3, #255
	ldr	r3, .L942+68
	str	r2, [r3, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #156
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	and	r2, r3, #255
	ldr	r3, .L942+68
	str	r2, [r3, #8]
	ldr	r3, [fp, #-20]
	add	r3, r3, #156
	ldr	r3, [r3, #0]
	and	r2, r3, #255
	ldr	r3, .L942+68
	str	r2, [r3, #12]
	ldr	r3, [fp, #-20]
	add	r3, r3, #160
	ldr	r2, [r3, #0]
	ldr	r3, .L942+28
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #164
	ldr	r2, [r3, #0]
	ldr	r3, .L942+36
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L907
.L908:
	ldr	r3, [fp, #-24]
	mov	r1, r3, asl #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #42
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+72
	str	r3, [r2, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #1
	add	r2, r3, #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #42
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #16
	ldr	r3, .L942+72
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L907:
	ldr	r3, [fp, #-24]
	cmp	r3, #15
	ble	.L908
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L909
.L943:
	.align	2
.L942:
	.word	point_now
	.word	config_static
	.word	coordinate_correct_able
	.word	id_first_coe
	.word	id_speed_coe
	.word	id_static_coe
	.word	soft_average
	.word	report_delay
	.word	153391689
	.word	shake_min
	.word	screen_y_max
	.word	screen_x_max
	.word	point_num_max
	.word	drv_num
	.word	sen_num
	.word	drv_num_nokey
	.word	sen_num_nokey
	.word	edge_cut
	.word	stretch_array
	.word	reset_mask_dis
	.word	reset_mask_type
	.word	edge_start
	.word	diagonal
	.word	point_extend
	.word	2584
	.word	67096
	.word	4293640
	.word	104956678
	.word	point_repeat
	.word	id_flag
	.word	4095
	.word	global_flag
	.word	near_set
	.word	report_ahead
	.word	edge_first
	.word	edge_first_coe
	.word	delay_key
	.word	report_delete
	.word	stretch_active
	.word	key_map_able
	.word	key_range_array
	.word	coordinate_correct_able
	.word	multi_x_array
	.word	multi_y_array
	.word	coordinate_correct_coe_x
	.word	coordinate_correct_coe_y
	.word	multi_group
	.word	filter_able
	.word	filter_coe
	.word	median_dis
	.word	ps_coe
	.word	pr_coe
	.word	average
	.word	shake_all_array
	.word	match_x
	.word	match_y
	.word	ignore_x
	.word	ignore_y
	.word	config_static
	.word	511
.L910:
	ldr	r3, [fp, #-24]
	mov	r1, r3, asl #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #58
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+212
	str	r3, [r2, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #1
	add	r2, r3, #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #58
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #16
	ldr	r3, .L942+212
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L909:
	ldr	r3, [fp, #-24]
	cmp	r3, #7
	ble	.L910
	ldr	r3, [fp, #-20]
	add	r3, r3, #264
	ldr	r2, [r3, #0]
	ldr	r3, .L942+132
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #288
	ldr	r2, [r3, #0]
	ldr	r3, .L942+136
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #292
	ldr	r2, [r3, #0]
	ldr	r3, .L942+140
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #296
	ldr	r2, [r3, #0]
	ldr	r3, .L942+144
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #300
	ldr	r2, [r3, #0]
	ldr	r3, .L942+148
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L911
.L912:
	ldr	r3, [fp, #-24]
	mov	r1, r3, asl #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #80
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L942+152
	str	r3, [r2, r1, asl #2]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #1
	add	r2, r3, #1
	ldr	r3, [fp, #-24]
	add	r3, r3, #80
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #16
	ldr	r3, .L942+152
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L911:
	ldr	r3, [fp, #-24]
	cmp	r3, #15
	ble	.L912
	ldr	r3, [fp, #-20]
	add	r3, r3, #384
	ldr	r2, [r3, #0]
	ldr	r3, .L942+156
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L913
.L914:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #97
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+160
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L913:
	ldr	r3, [fp, #-24]
	cmp	r3, #23
	ble	.L914
	ldr	r3, [fp, #-20]
	add	r3, r3, #1024
	ldr	r2, [r3, #0]
	ldr	r3, .L942+164
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L915
.L916:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #256
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+168
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #260
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+172
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L915:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L916
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L917
.L918:
	ldr	r1, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r3, r3, #264
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r2, [fp, #-24]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	ldr	r3, .L942+176
	str	r2, [r3, r1, asl #2]
	ldr	r1, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r3, r3, #280
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r2, [fp, #-24]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	ldr	r3, .L942+180
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L917:
	ldr	r3, [fp, #-24]
	cmp	r3, #63
	ble	.L918
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L919
.L920:
	ldr	r1, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	add	r3, r3, #296
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r2, [fp, #-24]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #30
	add	r2, r2, r3
	and	r2, r2, #3
	rsb	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r3, r0, lsr r3
	and	r2, r3, #255
	ldr	r3, .L942+184
	str	r2, [r3, r1, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L919:
	ldr	r3, [fp, #-24]
	cmp	r3, #255
	ble	.L920
	ldr	r3, [fp, #-20]
	add	r3, r3, #1536
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L942+188
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L921
.L922:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #384
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, .L942+192
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L921:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L922
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L923
.L924:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #388
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	and	r1, r3, #255
	ldr	r3, .L942+196
	strb	r1, [r3, r2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L923:
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	ble	.L924
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L925
.L926:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #392
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+200
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	add	r3, r3, #424
	add	r3, r3, #1
	mov	r3, r3, asl #2
	ldr	r1, [fp, #-20]
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L942+204
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L925:
	ldr	r3, [fp, #-24]
	cmp	r3, #31
	ble	.L926
	ldr	r3, [fp, #-20]
	add	r3, r3, #1824
	add	r3, r3, #4
	mov	r0, r3
	bl	GestureSet
.L906:
	ldr	r3, .L942+208
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L927
	ldr	r3, .L942+208
	mov	r2, #4
	str	r2, [r3, #0]
.L927:
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L928
.L931:
	ldr	r3, [fp, #-24]
	mov	r2, r3, asl #1
	ldr	r3, .L942+212
	ldr	r3, [r3, r2, asl #2]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L929
	ldr	r3, [fp, #-24]
	mov	r2, r3, asl #1
	ldr	r3, [fp, #-24]
	mov	r1, r3, asl #1
	ldr	r3, .L942+212
	ldr	r3, [r3, r1, asl #2]
	bic	r1, r3, #32768
	ldr	r3, .L942+212
	str	r1, [r3, r2, asl #2]
	b	.L930
.L929:
	ldr	r3, [fp, #-24]
	mov	r4, r3, asl #1
	ldr	r3, [fp, #-24]
	mov	r2, r3, asl #1
	ldr	r3, .L942+212
	ldr	r3, [r3, r2, asl #2]
	mov	r0, r3
	bl	Sqrt
	mov	r3, r0
	mov	r2, r3
	ldr	r3, .L942+212
	str	r2, [r3, r4, asl #2]
.L930:
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L928:
	ldr	r3, [fp, #-24]
	cmp	r3, #7
	ble	.L931
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L932
.L937:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+216
	ldr	r3, [r3, r2, asl #2]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L933
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+216
	ldr	r3, [r3, r1, asl #2]
	mvn	r3, r3, asl #16
	mvn	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L942+216
	str	r1, [r3, r2, asl #2]
.L933:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+220
	ldr	r3, [r3, r2, asl #2]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L934
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+220
	ldr	r3, [r3, r1, asl #2]
	mvn	r3, r3, asl #16
	mvn	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L942+220
	str	r1, [r3, r2, asl #2]
.L934:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+224
	ldr	r3, [r3, r2, asl #2]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L935
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+224
	ldr	r3, [r3, r1, asl #2]
	mvn	r3, r3, asl #16
	mvn	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L942+224
	str	r1, [r3, r2, asl #2]
.L935:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+228
	ldr	r3, [r3, r2, asl #2]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L936
	ldr	r2, [fp, #-24]
	ldr	r1, [fp, #-24]
	ldr	r3, .L942+228
	ldr	r3, [r3, r1, asl #2]
	mvn	r3, r3, asl #16
	mvn	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, .L942+228
	str	r1, [r3, r2, asl #2]
.L936:
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L932:
	ldr	r3, [fp, #-24]
	cmp	r3, #1
	ble	.L937
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L938
.L939:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+232
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
.L938:
	ldr	r2, [fp, #-24]
	ldr	r3, .L942+236
	cmp	r2, r3
	ble	.L939
	b	.L940
.L941:
	mov	r0, r0	@ nop
.L940:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
	.size	gsl_DataInit, .-gsl_DataInit
	.align	2
	.global	gsl_version_id
	.type	gsl_version_id, %function
gsl_version_id:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	ldr	r3, .L946
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L947:
	.align	2
.L946:
	.word	538247449
	.size	gsl_version_id, .-gsl_version_id
	.align	2
	.global	gsl_mask_tiaoping
	.type	gsl_mask_tiaoping, %function
gsl_mask_tiaoping:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	ldr	r3, .L950
	ldr	r3, [r3, #0]
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L951:
	.align	2
.L950:
	.word	reset_mask_send
	.size	gsl_mask_tiaoping, .-gsl_mask_tiaoping
	.align	2
	.type	GetFlag, %function
GetFlag:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-12]
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #32768
	cmp	r3, #0
	beq	.L953
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #-16777216
	cmp	r3, #1509949440
	bne	.L953
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	mov	r3, r3, asr #16
	and	r2, r3, #255
	ldr	r3, .L970+4
	str	r2, [r3, #0]
.L953:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #256
	cmp	r3, #0
	bne	.L954
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #512
	cmp	r3, #0
	beq	.L955
	ldr	r3, .L970+8
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #128
	cmp	r3, #0
	beq	.L955
.L954:
	mov	r0, #0
	bl	gsl_id_reg_init
.L955:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #768
	cmp	r3, #0
	bne	.L956
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	orr	r3, r3, #-2147483648
	str	r3, [r2, #0]
.L956:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #1024
	cmp	r3, #0
	beq	.L957
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	orr	r3, r3, #536870912
	str	r3, [r2, #0]
	b	.L958
.L957:
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	bic	r3, r3, #536870912
	str	r3, [r2, #0]
.L958:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #8192
	cmp	r3, #0
	beq	.L959
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	orr	r3, r3, #251658240
	str	r3, [r2, #0]
	b	.L960
.L959:
	ldr	r3, .L970+8
	ldrb	r3, [r3, #3]	@ zero_extendqisi2
	and	r3, r3, #15
	cmp	r3, #0
	beq	.L960
	ldr	r3, .L970+8
	ldrb	r3, [r3, #3]
	and	r3, r3, #15
	and	r3, r3, #255
	add	r3, r3, #15
	and	r3, r3, #15
	and	r2, r3, #255
	ldr	r1, .L970+8
	ldr	r3, [r1, #0]
	and	r2, r2, #15
	bic	r3, r3, #251658240
	mov	r2, r2, asl #24
	orr	r3, r2, r3
	str	r3, [r1, #0]
.L960:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #16384
	cmp	r3, #0
	beq	.L961
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	orr	r3, r3, #8388608
	str	r3, [r2, #0]
	b	.L962
.L961:
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	bic	r3, r3, #8388608
	str	r3, [r2, #0]
.L962:
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L963
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #2048
	cmp	r3, #0
	beq	.L964
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	orr	r3, r3, #268435456
	str	r3, [r2, #0]
	b	.L963
.L964:
	ldr	r2, .L970+8
	ldr	r3, [r2, #0]
	bic	r3, r3, #268435456
	str	r3, [r2, #0]
.L963:
	ldr	r3, .L970+12
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	ldr	r3, .L970+12
	str	r2, [r3, #0]
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #16
	ldr	r3, .L970+16
	str	r2, [r3, #0]
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	and	r3, r3, #255
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-8]
	cmp	r3, #10
	ble	.L965
	mov	r3, #10
	str	r3, [fp, #-8]
.L965:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L966
.L968:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	blt	.L967
	ldr	r2, [fp, #-12]
	ldr	r3, .L970+20
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L967:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L966:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	ble	.L968
	ldr	r3, .L970
	ldr	r3, [r3, #0]
	bic	r2, r3, #255
	ldr	r3, [fp, #-8]
	add	r2, r2, r3
	ldr	r3, .L970
	str	r2, [r3, #0]
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L971:
	.align	2
.L970:
	.word	point_num
	.word	gesture_last
	.word	global_state
	.word	inte_count
	.word	csensor_count
	.word	point_now
	.size	GetFlag, .-GetFlag
	.align	2
	.type	PointIgnore, %function
PointIgnore:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	ldr	r3, .L994
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L991
.L973:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L975
.L985:
	ldr	r2, [fp, #-16]
	ldr	r1, .L994+4
	mov	r3, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L992
.L976:
	ldr	r3, [fp, #-16]
	ldr	r2, .L994+4
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L994+8
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L994+12
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-8]
	ldr	r2, [fp, #-16]
	ldr	r1, .L994+4
	mov	r3, #2
	mov	r2, r2, asl #2
	add	r2, r1, r2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]
	ldr	r2, .L994+16
	ldr	r1, [r2, #0]
	mul	r2, r1, r3
	ldr	r3, .L994+20
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #6
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-12]
	ldr	r3, .L994+24
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L978
	ldr	r3, .L994+24
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L979
.L978:
	ldr	r3, .L994+24
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	ble	.L980
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L980:
	ldr	r3, .L994+24
	ldr	r2, [r3, #4]
	ldr	r3, .L994+8
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	bgt	.L981
	ldr	r3, .L994+8
	ldr	r2, [r3, #0]
	ldr	r3, .L994+24
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L981
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L981:
	ldr	r3, .L994+24
	ldr	r2, [r3, #4]
	ldr	r3, .L994+8
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	blt	.L979
	ldr	r3, .L994+24
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	cmp	r2, r3
	bge	.L979
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L979:
	ldr	r3, .L994+28
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L982
	ldr	r3, .L994+28
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L977
.L982:
	ldr	r3, .L994+28
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	ble	.L983
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L983:
	ldr	r3, .L994+28
	ldr	r2, [r3, #4]
	ldr	r3, .L994+16
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	bgt	.L984
	ldr	r3, .L994+16
	ldr	r2, [r3, #0]
	ldr	r3, .L994+28
	ldr	r3, [r3, #4]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L984
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
.L984:
	ldr	r3, .L994+28
	ldr	r2, [r3, #4]
	ldr	r3, .L994+16
	ldr	r3, [r3, #0]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	cmp	r2, r3
	blt	.L977
	ldr	r3, .L994+28
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bge	.L977
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	b	.L977
.L992:
	mov	r0, r0	@ nop
.L977:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L975:
	ldr	r3, .L994+32
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	blt	.L985
	mov	r3, #0
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L986
.L989:
	ldr	r2, [fp, #-16]
	ldr	r3, .L994+4
	ldr	r3, [r3, r2, asl #2]
	cmp	r3, #0
	beq	.L993
.L987:
	ldr	r2, [fp, #-12]
	ldr	r0, [fp, #-16]
	ldr	r3, .L994+4
	ldr	r1, .L994+4
	ldr	r1, [r1, r0, asl #2]
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
	b	.L988
.L993:
	mov	r0, r0	@ nop
.L988:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L986:
	ldr	r3, .L994+32
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	blt	.L989
	ldr	r3, .L994+32
	ldr	r2, [fp, #-12]
	str	r2, [r3, #0]
	b	.L990
.L991:
	mov	r0, r0	@ nop
.L990:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L995:
	.align	2
.L994:
	.word	id_flag
	.word	point_now
	.word	screen_y_max
	.word	sen_num_nokey
	.word	screen_x_max
	.word	drv_num_nokey
	.word	ignore_y
	.word	ignore_x
	.word	point_num
	.size	PointIgnore, .-PointIgnore
	.align	2
	.global	gsl_alg_id_main
	.type	gsl_alg_id_main, %function
gsl_alg_id_main:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r2, [r3, #120]
	ldr	r3, .L1005
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L997
.L998:
	ldr	r2, [fp, #-8]
	ldr	r1, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r1, r1, #20
	ldr	r3, [r3, r1, asl #2]
	mov	r1, r3, asl #28
	ldr	r0, [fp, #-8]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, r0, asl #2]
	mov	r3, r3, asl #16
	orr	r1, r1, r3
	ldr	r0, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r0, r0, #10
	ldr	r3, [r3, r0, asl #2]
	orr	r3, r1, r3
	mov	r1, r3
	ldr	r3, .L1005+4
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L997:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L998
	bl	GestureJoint
	mov	r3, r0
	cmp	r3, #0
	beq	.L999
	ldr	r3, [fp, #-16]
	mov	r2, #0
	str	r2, [r3, #120]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1000
.L1001:
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r2, r2, #20
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r2, r2, #10
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1000:
	ldr	r3, [fp, #-8]
	cmp	r3, #9
	ble	.L1001
	b	.L1004
.L999:
	bl	GetFlag
	bl	DataCheck
	mov	r3, r0
	cmp	r3, #0
	bne	.L1003
	ldr	r3, .L1005
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	mov	r2, #0
	str	r2, [r3, #120]
	b	.L1004
.L1003:
	bl	PressureSave
	ldr	r3, .L1005
	ldr	r3, [r3, #0]
	and	r2, r3, #255
	ldr	r3, .L1005
	str	r2, [r3, #0]
	bl	PointIgnore
	bl	PointCoor
	bl	CoordinateCorrect
	bl	PointEdge
	bl	PointRepeat
	ldr	r0, .L1005+4
	bl	GetPointNum
	bl	PointPointer
	bl	PointPredict
	bl	PointId
	bl	PointNewId
	bl	PointOrder
	bl	PointCross
	ldr	r3, .L1005+8
	ldr	r3, [r3, #0]
	mov	r0, r3
	bl	GetPointNum
	bl	DoubleClick
	ldr	r3, .L1005
	ldr	r2, [r3, #0]
	ldr	r3, .L1005+12
	str	r2, [r3, #0]
	bl	ResetMask
	bl	PointStretch
	bl	PointDiagonal
	bl	PointFilter
	ldr	r3, .L1005+16
	ldr	r3, [r3, #0]
	mov	r0, r3
	bl	GetPointNum
	ldr	r3, .L1005+16
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1005
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	GestureMain
	bl	PointDelay
	bl	PointMenu
	bl	PointExtend
	bl	PointPressure
	ldr	r0, [fp, #-16]
	bl	PointReport
.L1004:
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1006:
	.align	2
.L1005:
	.word	point_num
	.word	point_now
	.word	point_pointer
	.word	prev_num
	.word	point_report
	.size	gsl_alg_id_main, .-gsl_alg_id_main
	.align	2
	.type	DoubleClick, %function
DoubleClick:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #20
	ldr	r3, .L1037
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L1037+4
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bgt	.L1008
	ldr	r3, .L1037+4
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L1009
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1008
.L1009:
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #16
	cmp	r3, #0
	beq	.L1010
.L1008:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L1011
.L1012:
	ldr	r2, [fp, #-16]
	ldr	r3, .L1037+12
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L1011:
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	bls	.L1012
	b	.L1030
.L1010:
	ldr	r3, .L1037+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1014
	ldr	r3, .L1037+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1014
	mov	r3, #3
	str	r3, [fp, #-16]
	b	.L1015
.L1016:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r1, r3, #1
	ldr	r3, .L1037+12
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, .L1037+12
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #1
	str	r3, [fp, #-16]
.L1015:
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bgt	.L1016
	ldr	r3, .L1037+20
	ldr	r2, [r3, #0]
	ldr	r3, .L1037+12
	str	r2, [r3, #0]
	mov	r3, #3
	str	r3, [fp, #-16]
	b	.L1017
.L1018:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r1, r3, #2
	ldr	r3, .L1037+24
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, .L1037+24
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #1
	str	r3, [fp, #-16]
.L1017:
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bgt	.L1018
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #0]
	ldr	r3, .L1037+24
	str	r2, [r3, #0]
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #0]
	ldr	r3, .L1037+24
	str	r2, [r3, #4]
.L1014:
	ldr	r3, .L1037+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1019
	ldr	r3, .L1037+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1019
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldr	r0, .L1037+28
	mov	r1, r3
	bl	PointDistance
	mov	r4, r0
	ldr	r0, .L1037+28
	ldr	r1, .L1037+24
	bl	PointDistance
	mov	r3, r0
	cmp	r4, r3
	bls	.L1019
	ldr	r3, .L1037+8
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #0]
	ldr	r3, .L1037+24
	str	r2, [r3, #0]
.L1019:
	ldr	r3, .L1037+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1030
	ldr	r3, .L1037+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1030
	mov	r3, #3
	str	r3, [fp, #-16]
	b	.L1020
.L1021:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r1, r3, #1
	ldr	r3, .L1037+12
	ldr	r1, [r3, r1, asl #2]
	ldr	r3, .L1037+12
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #1
	str	r3, [fp, #-16]
.L1020:
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bgt	.L1021
	ldr	r3, .L1037+20
	ldr	r2, [r3, #0]
	ldr	r3, .L1037+12
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L1022
.L1023:
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-16]
	ldr	r3, .L1037+12
	ldr	r2, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	add	r0, r3, #1
	ldr	r3, .L1037+12
	ldr	r3, [r3, r0, asl #2]
	rsb	r3, r3, r2
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mvn	r2, #15
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r2, r1, r2
	str	r3, [r2, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L1022:
	ldr	r3, [fp, #-16]
	cmp	r3, #2
	bls	.L1023
	ldr	r2, [fp, #-28]
	ldr	r3, .L1037+32
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L1037+36
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	cmp	r2, r3
	bcc	.L1031
	ldr	r2, [fp, #-20]
	ldr	r3, .L1037+32
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L1037+36
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	cmp	r2, r3
	bcc	.L1032
	ldr	r2, [fp, #-24]
	ldr	r3, .L1037+40
	ldr	r3, [r3, #0]
	mov	r1, r3
	ldr	r3, .L1037+36
	ldr	r0, [r3, #0]
	mul	r3, r0, r1
	cmp	r2, r3
	bhi	.L1033
.L1025:
	ldr	r0, .L1037+24
	ldr	r1, .L1037+28
	bl	PointDistance
	mov	r3, r0
	cmp	r3, #4096
	bhi	.L1034
	ldr	r0, .L1037+44
	ldr	r1, .L1037+48
	bl	PointDistance
	mov	r3, r0
	cmp	r3, #4096
	bhi	.L1035
	ldr	r0, .L1037+28
	ldr	r1, .L1037+48
	bl	PointDistance
	mov	r3, r0
	cmp	r3, #16384
	bhi	.L1036
.L1027:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L1028
.L1029:
	ldr	r2, [fp, #-16]
	ldr	r3, .L1037+12
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L1028:
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	bls	.L1029
	ldr	r3, .L1037
	mov	r2, #42
	str	r2, [r3, #0]
	b	.L1030
.L1031:
	mov	r0, r0	@ nop
	b	.L1030
.L1032:
	mov	r0, r0	@ nop
	b	.L1030
.L1033:
	mov	r0, r0	@ nop
	b	.L1030
.L1034:
	mov	r0, r0	@ nop
	b	.L1030
.L1035:
	mov	r0, r0	@ nop
	b	.L1030
.L1036:
	mov	r0, r0	@ nop
.L1030:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L1038:
	.align	2
.L1037:
	.word	double_click
	.word	point_num
	.word	point_pointer
	.word	click_count
	.word	prev_num
	.word	csensor_count
	.word	point_click
	.word	point_click+4
	.word	double_down
	.word	average
	.word	double_up
	.word	point_click+8
	.word	point_click+12
	.size	DoubleClick, .-DoubleClick
	.align	2
	.global	gsl_FunIICRead
	.type	gsl_FunIICRead, %function
gsl_FunIICRead:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	ldr	r3, .L1041
	ldr	r2, [fp, #-8]
	str	r2, [r3, #0]
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1042:
	.align	2
.L1041:
	.word	ReadIICInt
	.size	gsl_FunIICRead, .-gsl_FunIICRead
	.align	2
	.type	GestureJoint, %function
GestureJoint:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 528
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #528
	mov	r3, #0
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-8]
	ldr	r3, .L1065
	ldr	r3, [r3, #0]
	and	r3, r3, #32768
	cmp	r3, #0
	bne	.L1044
	mov	r3, #0
	b	.L1045
.L1044:
	ldr	r3, .L1065
	ldr	r3, [r3, #0]
	and	r3, r3, #-16777216
	cmp	r3, #1526726656
	beq	.L1046
	mov	r3, #0
	b	.L1045
.L1046:
	ldr	r3, .L1065+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1047
	mov	r3, #0
	b	.L1045
.L1047:
	ldr	r3, .L1065+8
	ldr	r3, [r3, #0]
	str	r3, [fp, #-16]
	ldr	r3, .L1065+8
	ldr	r3, [r3, #4]
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #128
	bls	.L1048
	mov	r3, #128
	str	r3, [fp, #-20]
.L1048:
	ldr	r3, .L1065+4
	ldr	r3, [r3, #0]
	sub	r2, fp, #532
	mov	r0, r2
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-20]
	mov	lr, pc
	bx	r3
	ldr	r3, [fp, #-20]
	cmp	r3, #7
	bhi	.L1049
	ldr	r3, .L1065+12
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	mov	r3, #1
	b	.L1045
.L1049:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1050
.L1051:
	ldr	r2, [fp, #-12]
	ldr	r3, .L1065+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1050:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	bls	.L1051
	bl	GestureInit
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1052
.L1061:
	ldr	r2, [fp, #-12]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bcs	.L1053
	ldr	r3, .L1065
	mov	r2, #1
	str	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	ldr	r3, .L1065+16
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1065+8
	str	r2, [r3, #0]
	ldr	r2, [fp, #-12]
	ldr	r3, .L1065+16
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1054
	ldr	r2, [fp, #-12]
	ldr	r3, .L1065+16
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1065+20
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L1055
.L1054:
	bl	GestureInit
	b	.L1056
.L1055:
	ldr	r3, .L1065+8
	ldrb	r3, [r3, #1]	@ zero_extendqisi2
	and	r3, r3, #32
	cmp	r3, #0
	beq	.L1057
	mov	r3, #8192
	str	r3, [fp, #-8]
	b	.L1058
.L1057:
	mov	r3, #0
	str	r3, [fp, #-8]
.L1058:
	ldr	r2, .L1065+24
	ldr	r3, [r2, #0]
	orr	r3, r3, #536870912
	str	r3, [r2, #0]
	b	.L1059
.L1053:
	ldr	r2, .L1065+24
	ldr	r3, [r2, #0]
	bic	r3, r3, #536870912
	str	r3, [r2, #0]
	ldr	r3, .L1065
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L1065+8
	mov	r2, #0
	str	r2, [r3, #0]
.L1059:
	bl	PointCoor
	bl	CoordinateCorrect
	bl	PointEdge
	bl	PointPointer
	bl	PointPredict
	bl	PointId
	bl	PointNewId
	bl	PointOrder
	bl	PointStretch
	bl	PointFilter
	ldr	r3, .L1065+28
	ldr	r3, [r3, #0]
	mov	r0, r3
	bl	GetPointNum
	ldr	r3, .L1065+28
	ldr	r3, [r3, #0]
	ldr	r2, .L1065+28
	ldr	r2, [r2, #0]
	ldr	r1, [r2, #0]
	ldr	r2, [fp, #-8]
	add	r2, r1, r2
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r2, r3, #2
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bls	.L1060
	ldr	r3, .L1065+28
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1065
	ldr	r3, [r3, #0]
	mov	r0, r2
	mov	r1, r3
	bl	GestureMain
.L1060:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1052:
	ldr	r3, [fp, #-20]
	add	r2, r3, #5
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	bhi	.L1061
.L1056:
	ldr	r3, .L1065
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1062
.L1063:
	ldr	r2, [fp, #-12]
	ldr	r3, .L1065+8
	mov	r1, #0
	str	r1, [r3, r2, asl #2]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1062:
	ldr	r3, [fp, #-12]
	cmp	r3, #9
	bls	.L1063
	mov	r3, #1
.L1045:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1066:
	.align	2
.L1065:
	.word	point_num
	.word	ReadIICInt
	.word	point_now
	.word	gesture_last
	.word	-528
	.word	-536834048
	.word	global_state
	.word	point_report
	.size	GestureJoint, .-GestureJoint
	.align	2
	.global	gsl_obtain_gesture
	.type	gsl_obtain_gesture, %function
gsl_obtain_gesture:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	bl	GestureDeal
	mov	r3, r0
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
	.size	gsl_obtain_gesture, .-gsl_obtain_gesture
	.align	2
	.type	GestureMain, %function
GestureMain:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L1085
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L1085+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1070
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L1071
.L1070:
	mov	r3, #0
	b	.L1072
.L1071:
	ldr	r0, [fp, #-16]
	bl	GestureMulti
	mov	r2, r0
	ldr	r3, .L1085+8
	str	r2, [r3, #0]
	ldr	r3, .L1085+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1073
	ldr	r3, .L1085
	mov	r2, #2
	str	r2, [r3, #0]
	mov	r3, #1
	b	.L1072
.L1073:
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bne	.L1074
	ldr	r3, .L1085+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1075
	mov	r3, #0
	b	.L1072
.L1075:
	ldr	r3, .L1085+12
	ldr	r3, [r3, #0]
	cmp	r3, #8
	bgt	.L1076
	bl	GestureInit
	mov	r3, #0
	b	.L1072
.L1076:
	ldr	r3, .L1085
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	mov	r3, #1
	b	.L1072
.L1074:
	ldr	r3, .L1085+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bge	.L1077
	mov	r3, #0
	b	.L1072
.L1077:
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	bne	.L1078
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1078
	ldr	r3, [fp, #-16]
	mov	r0, r3
	bl	GesturePush
	mov	r3, #0
	b	.L1072
.L1078:
	ldr	r3, [fp, #-20]
	cmp	r3, #2
	bne	.L1079
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1079
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1080
.L1079:
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	bne	.L1081
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1081
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1081
.L1080:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1082
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1082
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	bic	r3, r3, #61440
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	bic	r2, r3, #61440
	ldr	r3, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-8]
	mov	r3, r3, lsr #1
	bic	r3, r3, #61440
	str	r3, [fp, #-8]
	b	.L1083
.L1082:
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	bic	r3, r3, #61440
	str	r3, [fp, #-8]
.L1083:
	ldr	r3, [fp, #-8]
	orr	r3, r3, #8192
	str	r3, [fp, #-8]
	sub	r3, fp, #8
	mov	r0, r3
	bl	GesturePush
	mov	r3, #0
	b	.L1072
.L1081:
	ldr	r3, .L1085+12
	mvn	r2, #0
	str	r2, [r3, #0]
	mov	r3, #0
.L1072:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1086:
	.align	2
.L1085:
	.word	gesture_deal
	.word	gesture_dis_min
	.word	gesture_multi
	.word	gesture_num
	.size	GestureMain, .-GestureMain
	.align	2
	.type	GestureSqrt, %function
GestureSqrt:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-12]
	mov	r3, #14
	str	r3, [fp, #-8]
	b	.L1088
.L1090:
	mov	r2, #1
	ldr	r3, [fp, #-8]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-12]
	add	r3, r2, r3
	mov	r1, #1
	ldr	r2, [fp, #-8]
	mov	r1, r1, asl r2
	ldr	r2, [fp, #-12]
	add	r1, r1, r2
	mul	r2, r1, r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	bgt	.L1089
	mov	r2, #1
	ldr	r3, [fp, #-8]
	mov	r3, r2, asl r3
	ldr	r2, [fp, #-12]
	orr	r3, r2, r3
	str	r3, [fp, #-12]
.L1089:
	ldr	r3, [fp, #-8]
	sub	r3, r3, #1
	str	r3, [fp, #-8]
.L1088:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	bge	.L1090
	ldr	r3, [fp, #-12]
	mov	r0, r3
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
	.size	GestureSqrt, .-GestureSqrt
	.align	2
	.type	GestureDistance, %function
GestureDistance:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	str	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L1093
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r2
	ldr	r2, [fp, #-8]
	ldrh	r2, [r2, #2]
	mov	r1, r2
	ldr	r2, [fp, #-12]
	ldrh	r2, [r2, #2]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r1, r3, r1
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r0, r3, r0
	mul	r3, r0, r1
	add	r3, r2, r3
	mov	r0, r3
	bl	GestureSqrt
	mov	r3, r0
	b	.L1094
.L1093:
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #2]
	rsb	r3, r3, r2
	ldr	r2, [fp, #-8]
	ldrh	r2, [r2, #2]
	mov	r1, r2
	ldr	r2, [fp, #-12]
	ldrh	r2, [r2, #2]
	rsb	r1, r2, r1
	mul	r2, r1, r3
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r1, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r1, r3, r1
	ldr	r3, [fp, #-8]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r0, r3
	ldr	r3, [fp, #-12]
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r0, r3, r0
	mul	r3, r0, r1
	add	r3, r2, r3
.L1094:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
	.size	GestureDistance, .-GestureDistance
	.align	2
	.type	GesturePush, %function
GesturePush:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	bic	r3, r3, #53248
	str	r3, [fp, #-8]
	ldr	r3, .L1105
	ldr	r3, [r3, #0]
	cmp	r3, #255
	ble	.L1097
	mov	r3, #0
	b	.L1098
.L1097:
	ldr	r3, .L1105+4
	ldr	r3, [r3, #0]
	and	r3, r3, #2
	cmp	r3, #0
	beq	.L1099
	ldr	r3, .L1105+8
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #6
	ldr	r3, [fp, #-8]
	mov	r3, r3, lsr #16
	rsb	r3, r3, r2
	mov	r2, r3, asl #16
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	and	r3, r3, #8192
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L1099:
	ldr	r3, .L1105+4
	ldr	r3, [r3, #0]
	and	r3, r3, #4
	cmp	r3, #0
	beq	.L1100
	ldr	r3, [fp, #-8]
	mov	r2, r3, lsr #16
	mov	r2, r2, asl #16
	ldr	r3, .L1105+12
	ldr	r3, [r3, #0]
	mov	r1, r3, asl #6
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, r1
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	and	r3, r3, #8192
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L1100:
	ldr	r3, .L1105+4
	ldr	r3, [r3, #0]
	and	r3, r3, #8
	cmp	r3, #0
	beq	.L1101
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r2, r3, asl #16
	ldr	r3, [fp, #-8]
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	and	r3, r3, #8192
	add	r3, r2, r3
	str	r3, [fp, #-8]
.L1101:
	ldr	r3, .L1105
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1102
	ldr	r1, .L1105+16
	ldr	r3, .L1105
	ldr	r3, [r3, #0]
	mov	r2, r3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r1, [fp, #-8]
	str	r1, [r2, #0]
	add	r2, r3, #1
	ldr	r3, .L1105
	str	r2, [r3, #0]
	mov	r3, #1
	b	.L1098
.L1102:
	sub	r2, fp, #8
	ldr	r1, .L1105+16
	ldr	r3, .L1105
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	mov	r2, #1
	bl	GestureDistance
	mov	r2, r0
	ldr	r3, .L1105+20
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bgt	.L1103
	mov	r3, #0
	b	.L1098
.L1103:
	ldr	r1, .L1105+16
	ldr	r3, .L1105
	ldr	r3, [r3, #0]
	mov	r2, r3
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r1, [fp, #-8]
	str	r1, [r2, #0]
	add	r2, r3, #1
	ldr	r3, .L1105
	str	r2, [r3, #0]
	mov	r3, #1
.L1098:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1106:
	.align	2
.L1105:
	.word	gesture_num
	.word	gesture_flag
	.word	drv_num_nokey
	.word	sen_num_nokey
	.word	config_static
	.word	gesture_dis_min
	.size	GesturePush, .-GesturePush
	.align	2
	.type	GestureInit, %function
GestureInit:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	ldr	r3, .L1113
	ldr	r2, [r3, #0]
	ldr	r3, .L1113+4
	str	r2, [r3, #0]
	ldr	r3, .L1113
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L1113+8
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, .L1113+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	blt	.L1108
	ldr	r3, .L1113+12
	ldr	r3, [r3, #0]
	cmp	r3, #64
	ble	.L1109
.L1108:
	ldr	r3, .L1113+12
	mov	r2, #2
	str	r2, [r3, #0]
.L1109:
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1110
.L1111:
	ldr	r2, [fp, #-8]
	ldr	r1, .L1113+16
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	ldr	r1, .L1113+20
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1110:
	ldr	r3, [fp, #-8]
	cmp	r3, #4
	ble	.L1111
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1114:
	.align	2
.L1113:
	.word	gesture_num
	.word	gesture_num_last
	.word	gesture_deal
	.word	gesture_dis_min
	.word	multi_x
	.word	multi_o
	.size	GestureInit, .-GestureInit
	.align	2
	.type	GestureStretch, %function
GestureStretch:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #28
	mov	r3, #0
	str	r3, [fp, #-36]
	ldr	r3, .L1134
	str	r3, [fp, #-32]
	mov	r3, #0
	str	r3, [fp, #-28]
	ldr	r3, .L1134
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r3, .L1134+4
	ldr	r3, [r3, #0]
	cmp	r3, #255
	ble	.L1116
	mov	r3, #0
	b	.L1117
.L1116:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L1118
.L1124:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-36]
	cmp	r2, r3
	bls	.L1119
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-36]
.L1119:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-32]
	cmp	r2, r3
	bcs	.L1120
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-32]
.L1120:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bls	.L1121
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-28]
.L1121:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bcs	.L1122
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-24]
.L1122:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #8192
	cmp	r3, #0
	beq	.L1123
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L1123:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L1118:
	ldr	r3, .L1134+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-20]
	cmp	r2, r3
	blt	.L1124
	ldr	r3, .L1134+4
	ldr	r3, [r3, #0]
	add	r2, r3, #3
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #2
	mov	r2, r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L1125
	ldr	r3, [fp, #-16]
	cmp	r3, #4
	ble	.L1126
.L1125:
	mov	r3, #0
	b	.L1117
.L1126:
	ldr	r3, .L1134+12
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-32]
	add	r2, r2, r3
	ldr	r3, [fp, #-36]
	cmp	r2, r3
	bcs	.L1127
	ldr	r3, .L1134+16
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, [fp, #-24]
	add	r2, r2, r3
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bcc	.L1128
.L1127:
	mov	r3, #0
	b	.L1117
.L1128:
	ldr	r3, .L1134+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1129
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-32]
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #2
	mov	r2, r3
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, .L1134+20
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcc	.L1130
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-24]
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #2
	mov	r2, r3
	ldr	r1, [fp, #-36]
	ldr	r3, [fp, #-32]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r2, r3
	ldr	r3, .L1134+20
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcs	.L1129
.L1130:
	mov	r3, #0
	b	.L1117
.L1129:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L1131
.L1132:
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-32]
	rsb	r3, r3, r2
	mov	r2, r3
	mov	r2, r2, asl #8
	rsb	r2, r3, r2
	ldr	r1, [fp, #-36]
	ldr	r3, [fp, #-32]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r4, #2]	@ movhi
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r2, .L1134+8
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r2
	mov	r2, r3
	mov	r2, r2, asl #8
	rsb	r2, r3, r2
	ldr	r1, [fp, #-28]
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r1
	mov	r0, r2
	mov	r1, r3
	bl	__aeabi_uidiv
	mov	r3, r0
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, [r4, #0]
	mov	r2, r2, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r2
	mov	r3, r3, ror #20
	str	r3, [r4, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L1131:
	ldr	r3, .L1134+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-20]
	cmp	r2, r3
	blt	.L1132
	mov	r3, #1
.L1117:
	mov	r0, r3
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L1135:
	.align	2
.L1134:
	.word	65535
	.word	gesture_num
	.word	config_static
	.word	gesture_min_x
	.word	gesture_min_y
	.word	size_ratio
	.size	GestureStretch, .-GestureStretch
	.align	2
	.type	GestureLength, %function
GestureLength:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-8]
	mov	r3, #1
	str	r3, [fp, #-12]
	b	.L1137
.L1138:
	ldr	r2, .L1140
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1140
	ldr	r3, [fp, #-12]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	mov	r2, #1
	bl	GestureDistance
	mov	r3, r0
	ldr	r2, [fp, #-8]
	add	r3, r2, r3
	str	r3, [fp, #-8]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1137:
	ldr	r3, .L1140+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-12]
	cmp	r2, r3
	blt	.L1138
	ldr	r3, [fp, #-8]
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1141:
	.align	2
.L1140:
	.word	config_static
	.word	gesture_num
	.size	GestureLength, .-GestureLength
	.align	2
	.type	GestureStandard, %function
GestureStandard:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #28
	mov	r3, #0
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-20]
	bl	GestureLength
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, .L1153
	ldr	r2, .L1153+4
	ldr	r2, [r2, #0]
	bic	r2, r2, #-268435456
	str	r2, [r3, #0]
	ldr	r3, .L1153+8
	ldr	r1, .L1153+4
	ldr	r2, .L1153+12
	ldr	r2, [r2, #0]
	sub	r2, r2, #1
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	bic	r2, r2, #-268435456
	str	r2, [r3, #0]
	mov	r3, #1
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L1143
.L1146:
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1153+4
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	mov	r0, r2
	mov	r1, r3
	mov	r2, #1
	bl	GestureDistance
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	mov	r2, r3
	mov	r2, r2, asl #5
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r0, [fp, #-36]
	mul	r3, r0, r1
	cmp	r2, r3
	bge	.L1152
.L1144:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
	ldr	r3, .L1153+12
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-32]
	cmp	r2, r3
	blt	.L1146
	b	.L1145
.L1152:
	mov	r0, r0	@ nop
.L1145:
	ldr	r3, .L1153+12
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-32]
	cmp	r2, r3
	bge	.L1151
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L1151
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-36]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r4, r3
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r1, .L1153+4
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r0, [fp, #-36]
	mul	r3, r0, r1
	ldr	r0, .L1153+16
	smull	ip, r1, r0, r3
	add	r1, r1, r3
	mov	r1, r1, asr #4
	mov	r3, r3, asr #31
	rsb	r1, r3, r1
	ldr	r3, [fp, #-20]
	rsb	r1, r3, r1
	ldr	r3, [fp, #-24]
	add	r1, r1, r3
	mul	r3, r1, r2
	mov	r0, r3
	ldr	r1, [fp, #-24]
	bl	__aeabi_idiv
	mov	r3, r0
	add	r3, r4, r3
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	bge	.L1148
	mov	r3, #0
	str	r3, [fp, #-28]
.L1148:
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-36]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [fp, #-28]
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r4, r3
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, .L1153+4
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r2, r3, r2
	ldr	r1, [fp, #-16]
	ldr	r0, [fp, #-36]
	mul	r3, r0, r1
	ldr	r0, .L1153+16
	smull	ip, r1, r0, r3
	add	r1, r1, r3
	mov	r1, r1, asr #4
	mov	r3, r3, asr #31
	rsb	r1, r3, r1
	ldr	r3, [fp, #-20]
	rsb	r1, r3, r1
	ldr	r3, [fp, #-24]
	add	r1, r1, r3
	mul	r3, r1, r2
	mov	r0, r3
	ldr	r1, [fp, #-24]
	bl	__aeabi_idiv
	mov	r3, r0
	add	r3, r4, r3
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	bge	.L1149
	mov	r3, #0
	str	r3, [fp, #-28]
.L1149:
	ldr	r2, .L1153+4
	ldr	r3, [fp, #-36]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-32]
	sub	r3, r3, #1
	str	r3, [fp, #-32]
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-24]
	rsb	r3, r3, r2
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L1143:
	ldr	r3, [fp, #-36]
	cmp	r3, #30
	ble	.L1144
.L1151:
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L1154:
	.align	2
.L1153:
	.word	config_static+1024
	.word	config_static
	.word	config_static+1148
	.word	gesture_num
	.word	-2078209981
	.size	GestureStandard, .-GestureStandard
	.align	2
	.type	GestureModel, %function
GestureModel:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 72
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #76
	str	r0, [fp, #-72]
	str	r1, [fp, #-76]
	str	r2, [fp, #-80]
	str	r3, [fp, #-84]
	ldr	r3, .L1178
	sub	ip, fp, #60
	mov	r4, r3
	ldmia	r4!, {r0, r1, r2, r3}
	stmia	ip!, {r0, r1, r2, r3}
	ldr	r3, [r4, #0]
	str	r3, [ip, #0]
	ldr	r3, [fp, #-72]
	cmp	r3, #0
	beq	.L1156
	ldr	r3, [fp, #-80]
	cmp	r3, #0
	ble	.L1156
	ldr	r3, [fp, #-76]
	cmp	r3, #0
	bgt	.L1157
.L1156:
	ldr	r3, [fp, #-84]
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	mvn	r3, #-2147483648
	b	.L1158
.L1157:
	mvn	r3, #-2147483648
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L1159
.L1171:
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L1160
.L1170:
	mov	r3, #0
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-40]
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L1161
.L1167:
	ldr	r2, [fp, #-28]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r0, fp, #12
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r3, r2, r3
	cmp	r3, #0
	blt	.L1175
	ldr	r2, [fp, #-28]
	mvn	r3, #47
	mov	r2, r2, asl #2
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r3, r2, r3
	cmp	r3, #31
	bgt	.L1176
.L1163:
	ldr	r3, [fp, #-36]
	and	r3, r3, #1
	cmp	r3, #0
	bne	.L1165
	ldr	r2, [fp, #-32]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-72]
	add	r2, r2, r3
	ldr	r3, [fp, #-36]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	add	r3, r3, #2
	ldr	r3, [r2, r3, asl #2]
	bic	r3, r3, #-16777216
	bic	r3, r3, #65280
	str	r3, [fp, #-64]
	b	.L1166
.L1165:
	ldr	r2, [fp, #-32]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-72]
	add	r2, r2, r3
	ldr	r3, [fp, #-36]
	mov	r1, r3, lsr #31
	add	r3, r1, r3
	mov	r3, r3, asr #1
	add	r3, r3, #2
	ldr	r3, [r2, r3, asl #2]
	mov	r3, r3, lsr #8
	bic	r3, r3, #-16777216
	bic	r3, r3, #65280
	str	r3, [fp, #-64]
.L1166:
	ldr	r2, .L1178+4
	ldr	r1, [fp, #-28]
	mvn	r3, #47
	mov	r1, r1, asl #2
	sub	r0, fp, #12
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r1, [r3, #0]
	ldr	r3, [fp, #-36]
	add	r3, r1, r3
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	sub	r3, fp, #64
	mov	r0, r2
	mov	r1, r3
	mov	r2, #0
	bl	GestureDistance
	mov	r3, r0
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
	b	.L1164
.L1175:
	mov	r0, r0	@ nop
	b	.L1164
.L1176:
	mov	r0, r0	@ nop
.L1164:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
.L1161:
	ldr	r3, [fp, #-36]
	cmp	r3, #31
	ble	.L1167
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L1177
.L1168:
	ldr	r0, [fp, #-40]
	ldr	r1, [fp, #-24]
	bl	__aeabi_idiv
	mov	r3, r0
	mov	r1, r3
	ldr	r2, [fp, #-32]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-72]
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	mul	r3, r2, r1
	add	r2, r3, #15
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #4
	str	r3, [fp, #-40]
	ldr	r2, [fp, #-40]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bge	.L1169
	ldr	r3, [fp, #-32]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-40]
	str	r3, [fp, #-20]
	b	.L1169
.L1177:
	mov	r0, r0	@ nop
.L1169:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
.L1160:
	ldr	r3, [fp, #-28]
	cmp	r3, #4
	bls	.L1170
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
.L1159:
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-76]
	cmp	r2, r3
	blt	.L1171
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-80]
	cmp	r2, r3
	bge	.L1172
	ldr	r2, [fp, #-16]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	mov	r3, r3, asl #3
	ldr	r2, [fp, #-72]
	add	r3, r2, r3
	ldr	r2, [r3, #4]
	ldr	r3, [fp, #-84]
	str	r2, [r3, #0]
	b	.L1173
.L1172:
	ldr	r3, [fp, #-84]
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
.L1173:
	ldr	r3, [fp, #-20]
.L1158:
	mov	r0, r3
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L1179:
	.align	2
.L1178:
	.word	C.1276.7974
	.word	config_static
	.size	GestureModel, .-GestureModel
	.align	2
	.type	ChangeXY, %function
ChangeXY:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1181
.L1183:
	ldr	r2, .L1185
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1185
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r1, r3, asl #16
	ldr	r0, .L1185
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	add	r1, r1, r3
	ldr	r0, .L1185
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #2
	add	r3, r0, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #8192
	add	r3, r1, r3
	str	r3, [r2, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1181:
	ldr	r3, .L1185+4
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-8]
	cmp	r2, r3
	bge	.L1184
	ldr	r3, [fp, #-8]
	cmp	r3, #255
	ble	.L1183
.L1184:
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1186:
	.align	2
.L1185:
	.word	config_static
	.word	gesture_num
	.size	ChangeXY, .-ChangeXY
	.align	2
	.type	GestureSet, %function
GestureSet:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	beq	.L1203
.L1188:
	ldr	r3, [fp, #-8]
	ldr	r3, [r3, #0]
	cmp	r3, #64
	bhi	.L1190
	ldr	r3, [fp, #-8]
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1204
	str	r2, [r3, #0]
	b	.L1191
.L1190:
	ldr	r3, .L1204
	mov	r2, #0
	str	r2, [r3, #0]
.L1191:
	ldr	r3, [fp, #-8]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1192
	ldr	r3, [fp, #-8]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1204+4
	str	r2, [r3, #0]
	b	.L1193
.L1192:
	ldr	r3, .L1204+4
	ldr	r2, .L1204+8
	str	r2, [r3, #0]
.L1193:
	ldr	r3, [fp, #-8]
	add	r3, r3, #8
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1204+4
	str	r2, [r3, #4]
	ldr	r3, [fp, #-8]
	add	r3, r3, #12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1194
	ldr	r3, [fp, #-8]
	add	r3, r3, #12
	ldr	r3, [r3, #0]
	b	.L1195
.L1194:
	mov	r3, #4
.L1195:
	ldr	r2, .L1204+12
	str	r3, [r2, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1196
	ldr	r3, [fp, #-8]
	add	r3, r3, #16
	ldr	r3, [r3, #0]
	b	.L1197
.L1196:
	mov	r3, #4
.L1197:
	ldr	r2, .L1204+16
	str	r3, [r2, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1198
	ldr	r3, .L1204+20
	mov	r2, #2
	str	r2, [r3, #0]
	ldr	r3, .L1204+24
	mov	r2, #30
	str	r2, [r3, #0]
	b	.L1199
.L1198:
	ldr	r3, [fp, #-8]
	add	r3, r3, #20
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L1204+20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #20
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L1204+24
	str	r2, [r3, #0]
.L1199:
	ldr	r3, [fp, #-8]
	add	r3, r3, #24
	ldr	r2, [r3, #0]
	ldr	r3, .L1204+28
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #28
	ldr	r2, [r3, #0]
	ldr	r3, .L1204+32
	str	r2, [r3, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #36
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1200
	ldr	r3, [fp, #-8]
	add	r3, r3, #36
	ldr	r3, [r3, #0]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, .L1204+36
	str	r3, [r2, #0]
	ldr	r3, [fp, #-8]
	add	r3, r3, #36
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, .L1204+40
	str	r2, [r3, #0]
	b	.L1201
.L1200:
	ldr	r3, .L1204+36
	mov	r2, #128
	str	r2, [r3, #0]
	ldr	r3, .L1204+40
	mov	r2, #192
	str	r2, [r3, #0]
.L1201:
	ldr	r3, [fp, #-8]
	add	r3, r3, #40
	ldr	r2, [r3, #0]
	ldr	r3, .L1204+44
	str	r2, [r3, #0]
	b	.L1202
.L1203:
	mov	r0, r0	@ nop
.L1202:
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1205:
	.align	2
.L1204:
	.word	gesture_dis_min
	.word	gesture_threshold
	.word	4095
	.word	x_scale
	.word	y_scale
	.word	double_down
	.word	double_up
	.word	multi_set
	.word	gesture_flag
	.word	gesture_min_x
	.word	gesture_min_y
	.word	size_ratio
	.size	GestureSet, .-GestureSet
	.align	2
	.type	GestureOrientation, %function
GestureOrientation:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L1219
.L1207:
	ldr	r3, [fp, #-16]
	cmp	r3, #2
	bne	.L1209
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1210
.L1211:
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	rsb	r3, r3, #255
	str	r3, [fp, #-8]
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [r2, #2]	@ movhi
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r3, [fp, #-8]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1210:
	ldr	r3, [fp, #-12]
	cmp	r3, #31
	ble	.L1211
	b	.L1218
.L1209:
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	beq	.L1212
	ldr	r3, [fp, #-16]
	cmp	r3, #3
	bne	.L1213
.L1212:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1214
.L1215:
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r1, .L1220
	ldr	r2, [fp, #-12]
	add	r2, r2, #256
	mov	r2, r2, asl #2
	add	r2, r1, r2
	ldrh	r2, [r2, #2]
	rsb	r2, r2, #255
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, #255
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1214:
	ldr	r3, [fp, #-12]
	cmp	r3, #31
	ble	.L1215
	b	.L1218
.L1213:
	ldr	r3, [fp, #-16]
	cmp	r3, #4
	bne	.L1218
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1216
.L1217:
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r3, r3, #255
	str	r3, [fp, #-8]
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r2, r2, r3
	ldr	r1, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r1, r3, lsr #16
	ldr	r3, [r2, #0]
	mov	r1, r1, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r1
	mov	r3, r3, ror #20
	str	r3, [r2, #0]
	ldr	r2, .L1220
	ldr	r3, [fp, #-12]
	add	r3, r3, #256
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r2, [fp, #-8]
	mov	r2, r2, asl #16
	mov	r2, r2, lsr #16
	strh	r2, [r3, #2]	@ movhi
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1216:
	ldr	r3, [fp, #-12]
	cmp	r3, #31
	ble	.L1217
	b	.L1218
.L1219:
	mov	r0, r0	@ nop
.L1218:
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1221:
	.align	2
.L1220:
	.word	config_static
	.size	GestureOrientation, .-GestureOrientation
	.align	2
	.type	GestureDeal, %function
GestureDeal:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #56
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1250
.L1223:
	ldr	r3, .L1257+4
	ldr	r2, [r3, #0]
	ldr	r3, .L1257
	str	r2, [r3, #0]
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1251
.L1225:
	ldr	r3, .L1257+8
	ldr	r3, [r3, #0]
	mov	r2, r3
	ldr	r3, .L1257
	str	r2, [r3, #0]
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1252
.L1226:
	ldr	r3, .L1257+12
	ldr	r3, [r3, #0]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L1227
	ldr	r3, .L1257+12
	ldr	r3, [r3, #0]
	bic	r2, r3, #1
	ldr	r3, .L1257+12
	str	r2, [r3, #0]
	bl	ChangeXY
.L1227:
	ldr	r3, .L1257+12
	ldr	r3, [r3, #0]
	and	r3, r3, #2
	cmp	r3, #0
	bne	.L1228
	mov	r3, #0
	b	.L1229
.L1228:
	ldr	r3, .L1257+12
	ldr	r3, [r3, #0]
	bic	r2, r3, #2
	ldr	r3, .L1257+12
	str	r2, [r3, #0]
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r3, #256
	cmp	r3, #0
	bne	.L1230
	bl	GestureLRUD
	mov	r2, r0
	ldr	r3, .L1257
	str	r2, [r3, #0]
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1253
.L1230:
	bl	GestureStretch
	mov	r3, r0
	cmp	r3, #0
	bne	.L1231
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r3, #256
	cmp	r3, #0
	beq	.L1254
	bl	GestureLRUD
	mov	r2, r0
	ldr	r3, .L1257
	str	r2, [r3, #0]
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	b	.L1224
.L1231:
	bl	GestureStandard
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1233
.L1240:
	ldr	r2, [fp, #-8]
	mvn	r3, #55
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	ldr	r0, [fp, #-8]
	bl	GestureOrientation
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r3, #240
	cmp	r3, #0
	bne	.L1234
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	beq	.L1235
.L1234:
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r3, #240
	cmp	r3, #0
	beq	.L1236
	mov	r2, #16
	ldr	r3, [fp, #-8]
	mov	r3, r2, asl r3
	mov	r2, r3
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L1236
.L1235:
	ldr	r3, .L1257+20
	ldr	r2, [r3, #0]
	sub	r3, fp, #20
	ldr	r0, .L1257+24
	mov	r1, #68
	bl	GestureModel
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, .L1257+28
	ldr	r0, [r3, #0]
	ldr	r3, .L1257+32
	ldr	r1, [r3, #0]
	ldr	r3, .L1257+20
	ldr	r2, [r3, #4]
	sub	r3, fp, #20
	add	r3, r3, #4
	bl	GestureModel
	mov	r3, r0
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L1237
.L1239:
	ldr	r2, [fp, #-12]
	mvn	r3, #23
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r1, [fp, #-8]
	mvn	r3, #55
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bgt	.L1238
	ldr	r1, [fp, #-8]
	ldr	r2, [fp, #-12]
	mvn	r3, #23
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	mvn	r3, #55
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
	ldr	r1, [fp, #-8]
	ldr	r2, [fp, #-12]
	mvn	r3, #15
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	mvn	r3, #39
	mov	r1, r1, asl #2
	sub	r0, fp, #4
	add	r1, r0, r1
	add	r3, r1, r3
	str	r2, [r3, #0]
.L1238:
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L1237:
	ldr	r3, [fp, #-12]
	cmp	r3, #1
	ble	.L1239
.L1236:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1233:
	ldr	r3, [fp, #-8]
	cmp	r3, #3
	ble	.L1240
	ldr	r3, .L1257+36
	mvn	r2, #-2147483648
	str	r2, [r3, #0]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1241
.L1243:
	ldr	r2, [fp, #-8]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #64
	ble	.L1242
	ldr	r2, [fp, #-8]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #90
	bgt	.L1242
	ldr	r2, [fp, #-8]
	mvn	r3, #55
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257+36
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bge	.L1242
	ldr	r2, [fp, #-8]
	mvn	r3, #55
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257+36
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257
	str	r2, [r3, #0]
.L1242:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1241:
	ldr	r3, [fp, #-8]
	cmp	r3, #3
	ble	.L1243
	ldr	r3, .L1257+36
	ldr	r3, [r3, #0]
	cmn	r3, #-2147483647
	bne	.L1255
.L1244:
	ldr	r3, .L1257+16
	ldr	r3, [r3, #0]
	and	r3, r3, #256
	cmp	r3, #0
	beq	.L1245
	bl	GestureLRUD
	mov	r2, r0
	ldr	r3, .L1257
	str	r2, [r3, #0]
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1256
.L1245:
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L1246
.L1248:
	ldr	r2, [fp, #-8]
	mvn	r3, #55
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257+36
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bge	.L1247
	ldr	r2, [fp, #-8]
	mvn	r3, #55
	mov	r2, r2, asl #2
	sub	r1, fp, #4
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257+36
	str	r2, [r3, #0]
	ldr	r2, [fp, #-8]
	mvn	r3, #39
	mov	r2, r2, asl #2
	sub	r0, fp, #4
	add	r2, r0, r2
	add	r3, r2, r3
	ldr	r2, [r3, #0]
	ldr	r3, .L1257
	str	r2, [r3, #0]
.L1247:
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L1246:
	ldr	r3, [fp, #-8]
	cmp	r3, #3
	ble	.L1248
	b	.L1224
.L1250:
	mov	r0, r0	@ nop
	b	.L1224
.L1251:
	mov	r0, r0	@ nop
	b	.L1224
.L1252:
	mov	r0, r0	@ nop
	b	.L1224
.L1253:
	mov	r0, r0	@ nop
	b	.L1224
.L1254:
	mov	r0, r0	@ nop
	b	.L1224
.L1255:
	mov	r0, r0	@ nop
	b	.L1224
.L1256:
	mov	r0, r0	@ nop
.L1224:
	bl	GestureInit
	ldr	r3, .L1257
	ldr	r3, [r3, #0]
	str	r3, [fp, #-12]
	ldr	r3, .L1257
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-12]
.L1229:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1258:
	.align	2
.L1257:
	.word	gesture_last
	.word	gesture_multi
	.word	double_click
	.word	gesture_deal
	.word	gesture_flag
	.word	gesture_threshold
	.word	model_default
	.word	model_extern
	.word	model_extern_len
	.word	gesture_value
	.size	GestureDeal, .-GestureDeal
	.align	2
	.global	gsl_GestureExtern
	.type	gsl_GestureExtern, %function
gsl_GestureExtern:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	ldr	r3, .L1261
	ldr	r2, [fp, #-8]
	str	r2, [r3, #0]
	ldr	r3, .L1261+4
	ldr	r2, [fp, #-12]
	str	r2, [r3, #0]
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1262:
	.align	2
.L1261:
	.word	model_extern
	.word	model_extern_len
	.size	gsl_GestureExtern, .-gsl_GestureExtern
	.align	2
	.global	gsl_GestureExternInt
	.type	gsl_GestureExternInt, %function
gsl_GestureExternInt:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	str	r0, [fp, #-8]
	str	r1, [fp, #-12]
	ldr	r2, [fp, #-8]
	ldr	r3, .L1265
	str	r2, [r3, #0]
	ldr	r3, .L1265+4
	ldr	r2, [fp, #-12]
	str	r2, [r3, #0]
	add	sp, fp, #0
	ldmfd	sp!, {fp}
	bx	lr
.L1266:
	.align	2
.L1265:
	.word	model_extern
	.word	model_extern_len
	.size	gsl_GestureExternInt, .-gsl_GestureExternInt
	.align	2
	.type	GestureLRUD, %function
GestureLRUD:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #56
	mov	r3, #0
	str	r3, [fp, #-60]
	mov	r3, #0
	str	r3, [fp, #-56]
	mov	r3, #5
	str	r3, [fp, #-20]
	ldr	r3, .L1312
	ldr	r3, [r3, #0]
	add	r2, r3, #7
	cmp	r3, #0
	movlt	r3, r2
	mov	r3, r3, asr #3
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #1
	rsb	r2, r3, #0
	ldr	r3, .L1312
	ldr	r3, [r3, #0]
	add	r3, r2, r3
	sub	r3, r3, #1
	str	r3, [fp, #-12]
	mov	r3, #0
	str	r3, [fp, #-8]
	ldr	r3, .L1312+4
	ldr	r3, [r3, #0]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L1268
	ldr	r3, .L1312+4
	ldr	r3, [r3, #0]
	bic	r2, r3, #1
	ldr	r3, .L1312+4
	str	r2, [r3, #0]
	bl	ChangeXY
.L1268:
	ldr	r3, .L1312+4
	ldr	r3, [r3, #0]
	and	r3, r3, #4
	cmp	r3, #0
	bne	.L1269
	mov	r3, #0
	b	.L1270
.L1269:
	ldr	r3, .L1312+4
	ldr	r3, [r3, #0]
	bic	r2, r3, #4
	ldr	r3, .L1312+4
	str	r2, [r3, #0]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	add	r3, r2, r3
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-52]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-12]
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	add	r3, r2, r3
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	str	r3, [fp, #-48]
	ldr	r3, .L1312+12
	ldr	r3, [r3, #0]
	mov	r0, r3, asl #6
	ldr	r3, .L1312+16
	ldr	r3, [r3, #0]
	cmp	r3, #100
	ble	.L1271
	ldr	r3, .L1312+16
	ldr	r1, [r3, #0]
	ldr	r2, .L1312+20
	smull	ip, r3, r2, r1
	mov	r2, r3, asr #5
	mov	r3, r1, asr #31
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #2
	rsb	r2, r3, r1
	b	.L1272
.L1271:
	ldr	r3, .L1312+16
	ldr	r2, [r3, #0]
.L1272:
	mov	r1, r2
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, .L1312+24
	ldr	r3, [r3, #0]
	mov	r0, r3, asl #6
	ldr	r3, .L1312+28
	ldr	r3, [r3, #0]
	cmp	r3, #100
	ble	.L1273
	ldr	r3, .L1312+28
	ldr	r1, [r3, #0]
	ldr	r2, .L1312+20
	smull	ip, r3, r2, r1
	mov	r2, r3, asr #5
	mov	r3, r1, asr #31
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #2
	rsb	r2, r3, r1
	b	.L1274
.L1273:
	ldr	r3, .L1312+28
	ldr	r2, [r3, #0]
.L1274:
	mov	r1, r2
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-40]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-44]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-32]
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-36]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-60]
	b	.L1275
.L1288:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-52]
	rsb	r3, r3, r2
	eor	r2, r3, r3, asr #31
	sub	r2, r2, r3, asr #31
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	bge	.L1276
	ldr	r3, [fp, #-56]
	orr	r3, r3, #1
	str	r3, [fp, #-56]
	b	.L1277
.L1276:
	ldr	r3, [fp, #-56]
	orr	r3, r3, #2
	str	r3, [fp, #-56]
.L1277:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-48]
	rsb	r3, r3, r2
	eor	r2, r3, r3, asr #31
	sub	r2, r2, r3, asr #31
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bge	.L1278
	ldr	r3, [fp, #-56]
	orr	r3, r3, #16
	str	r3, [fp, #-56]
	b	.L1279
.L1278:
	ldr	r3, [fp, #-56]
	orr	r3, r3, #32
	str	r3, [fp, #-56]
.L1279:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-60]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	ble	.L1280
	ldr	r3, [fp, #-56]
	orr	r3, r3, #256
	str	r3, [fp, #-56]
	b	.L1281
.L1280:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-60]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #2]
	rsb	r2, r3, r2
	ldr	r3, [fp, #-20]
	rsb	r3, r3, #0
	cmp	r2, r3
	bge	.L1281
	ldr	r3, [fp, #-56]
	orr	r3, r3, #512
	str	r3, [fp, #-56]
.L1281:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-60]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r2, r3, r2
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	ble	.L1282
	ldr	r3, [fp, #-56]
	orr	r3, r3, #4096
	str	r3, [fp, #-56]
	b	.L1283
.L1282:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r1, .L1312+8
	ldr	r3, [fp, #-60]
	sub	r3, r3, #1
	mov	r3, r3, asl #2
	add	r3, r1, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	rsb	r2, r3, r2
	ldr	r3, [fp, #-20]
	rsb	r3, r3, #0
	cmp	r2, r3
	bge	.L1283
	ldr	r3, [fp, #-56]
	orr	r3, r3, #8192
	str	r3, [fp, #-56]
.L1283:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bge	.L1284
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-40]
.L1284:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r2, r3
	ldr	r3, [fp, #-44]
	cmp	r2, r3
	ble	.L1285
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	str	r3, [fp, #-44]
.L1285:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-32]
	cmp	r2, r3
	bge	.L1286
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-32]
.L1286:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	mov	r2, r3
	ldr	r3, [fp, #-36]
	cmp	r2, r3
	ble	.L1287
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	str	r3, [fp, #-36]
.L1287:
	ldr	r3, [fp, #-60]
	add	r3, r3, #1
	str	r3, [fp, #-60]
.L1275:
	ldr	r2, [fp, #-60]
	ldr	r3, [fp, #-12]
	cmp	r2, r3
	ble	.L1288
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+32
	cmp	r2, r3
	beq	.L1289
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+36
	cmp	r2, r3
	beq	.L1289
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+40
	cmp	r2, r3
	beq	.L1289
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+44
	cmp	r2, r3
	bne	.L1290
.L1289:
	ldr	r3, .L1312+28
	ldr	r3, [r3, #0]
	add	r3, r3, #99
	cmp	r3, #198
	bls	.L1291
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-32]
	rsb	r2, r3, r2
	ldr	r3, .L1312+28
	ldr	r3, [r3, #0]
	ldr	r0, .L1312+20
	smull	ip, r1, r0, r3
	mov	r1, r1, asr #5
	mov	r3, r3, asr #31
	rsb	r3, r3, r1
	mov	r3, r3, asl #6
	cmp	r2, r3
	bge	.L1309
.L1291:
	ldr	r3, .L1312+48
	str	r3, [fp, #-8]
	b	.L1293
.L1290:
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+52
	cmp	r2, r3
	beq	.L1294
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+56
	cmp	r2, r3
	beq	.L1294
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+60
	cmp	r2, r3
	beq	.L1294
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+64
	cmp	r2, r3
	bne	.L1295
.L1294:
	ldr	r3, .L1312+28
	ldr	r3, [r3, #0]
	add	r3, r3, #99
	cmp	r3, #198
	bls	.L1296
	ldr	r2, [fp, #-36]
	ldr	r3, [fp, #-32]
	rsb	r2, r3, r2
	ldr	r3, .L1312+28
	ldr	r3, [r3, #0]
	ldr	r0, .L1312+20
	smull	ip, r1, r0, r3
	mov	r1, r1, asr #5
	mov	r3, r3, asr #31
	rsb	r3, r3, r1
	mov	r3, r3, asl #6
	cmp	r2, r3
	bge	.L1310
.L1296:
	ldr	r3, .L1312+68
	str	r3, [fp, #-8]
	b	.L1293
.L1295:
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+72
	cmp	r2, r3
	beq	.L1298
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+76
	cmp	r2, r3
	beq	.L1298
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+80
	cmp	r2, r3
	beq	.L1298
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+80
	cmp	r2, r3
	bne	.L1299
.L1298:
	ldr	r3, .L1312+16
	ldr	r3, [r3, #0]
	add	r3, r3, #99
	cmp	r3, #198
	bls	.L1300
	ldr	r2, [fp, #-44]
	ldr	r3, [fp, #-40]
	rsb	r2, r3, r2
	ldr	r3, .L1312+16
	ldr	r3, [r3, #0]
	ldr	r0, .L1312+20
	smull	ip, r1, r0, r3
	mov	r1, r1, asr #5
	mov	r3, r3, asr #31
	rsb	r3, r3, r1
	mov	r3, r3, asl #6
	cmp	r2, r3
	bge	.L1311
.L1300:
	ldr	r3, .L1312+84
	str	r3, [fp, #-8]
	b	.L1293
.L1299:
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+88
	cmp	r2, r3
	beq	.L1302
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+92
	cmp	r2, r3
	beq	.L1302
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+96
	cmp	r2, r3
	beq	.L1302
	ldr	r2, [fp, #-56]
	ldr	r3, .L1312+100
	cmp	r2, r3
	bne	.L1293
.L1302:
	ldr	r3, .L1312+16
	ldr	r3, [r3, #0]
	add	r3, r3, #99
	cmp	r3, #198
	bls	.L1303
	ldr	r2, [fp, #-44]
	ldr	r3, [fp, #-40]
	rsb	r2, r3, r2
	ldr	r3, .L1312+16
	ldr	r3, [r3, #0]
	ldr	r0, .L1312+20
	smull	ip, r1, r0, r3
	mov	r1, r1, asr #5
	mov	r3, r3, asr #31
	rsb	r3, r3, r1
	mov	r3, r3, asl #6
	cmp	r2, r3
	bge	.L1293
.L1303:
	ldr	r3, .L1312+104
	str	r3, [fp, #-8]
	b	.L1293
.L1309:
	mov	r0, r0	@ nop
	b	.L1293
.L1310:
	mov	r0, r0	@ nop
	b	.L1293
.L1311:
	mov	r0, r0	@ nop
.L1293:
	ldr	r3, [fp, #-8]
	cmp	r3, #0
	ble	.L1304
	mov	r3, #0
	str	r3, [fp, #-56]
	mov	r3, #0
	str	r3, [fp, #-60]
	b	.L1305
.L1307:
	ldr	r2, .L1312+8
	ldr	r3, [fp, #-60]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	and	r3, r3, #8192
	cmp	r3, #0
	beq	.L1306
	ldr	r3, [fp, #-56]
	add	r3, r3, #1
	str	r3, [fp, #-56]
.L1306:
	ldr	r3, [fp, #-60]
	add	r3, r3, #1
	str	r3, [fp, #-60]
.L1305:
	ldr	r3, .L1312
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-60]
	cmp	r2, r3
	blt	.L1307
	ldr	r3, .L1312
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #31
	add	r3, r2, r3
	mov	r3, r3, asr #1
	mov	r2, r3
	ldr	r3, [fp, #-56]
	cmp	r2, r3
	bgt	.L1304
	ldr	r3, [fp, #-56]
	cmp	r3, #3
	ble	.L1304
	ldr	r3, [fp, #-8]
	sub	r3, r3, #4
	str	r3, [fp, #-8]
.L1304:
	ldr	r3, [fp, #-8]
.L1270:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1313:
	.align	2
.L1312:
	.word	gesture_num
	.word	gesture_deal
	.word	config_static
	.word	sen_num_nokey
	.word	x_scale
	.word	1374389535
	.word	drv_num_nokey
	.word	y_scale
	.word	8241
	.word	8497
	.word	8753
	.word	9009
	.word	41468
	.word	4145
	.word	4401
	.word	4657
	.word	4913
	.word	41469
	.word	531
	.word	4627
	.word	8723
	.word	41467
	.word	275
	.word	4371
	.word	8467
	.word	12563
	.word	41466
	.size	GestureLRUD, .-GestureLRUD
	.align	2
	.type	GestureMulti, %function
GestureMulti:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #48
	str	r0, [fp, #-48]
	ldr	r3, [fp, #-48]
	str	r3, [fp, #-40]
	mov	r3, #0
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-32]
	mov	r3, #0
	str	r3, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-24]
	ldr	r3, .L1351
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1315
	mov	r3, #5
	str	r3, [fp, #-16]
	mov	r3, #8
	str	r3, [fp, #-12]
	mov	r3, #16
	str	r3, [fp, #-8]
	b	.L1316
.L1315:
	ldr	r3, .L1351
	ldr	r3, [r3, #0]
	and	r3, r3, #255
	str	r3, [fp, #-16]
	ldr	r3, .L1351
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #8
	and	r3, r3, #255
	str	r3, [fp, #-12]
	ldr	r3, .L1351
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #16
	and	r3, r3, #255
	str	r3, [fp, #-8]
.L1316:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L1317
.L1320:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1345
.L1318:
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldrh	r3, [r3, #2]
	ldr	r2, [fp, #-32]
	add	r3, r2, r3
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldrh	r3, [r3, #0]	@ movhi
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	ldr	r2, [fp, #-28]
	add	r3, r2, r3
	str	r3, [fp, #-28]
	b	.L1319
.L1345:
	mov	r0, r0	@ nop
.L1319:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L1317:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L1320
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	bne	.L1321
	mov	r3, #0
	str	r3, [fp, #-24]
	ldr	r3, .L1351+4
	ldr	r3, [r3, #36]
	cmp	r3, #0
	blt	.L1322
	ldr	r3, .L1351+4
	ldr	r3, [r3, #28]
	cmp	r3, #0
	blt	.L1322
	ldr	r3, .L1351+4
	ldr	r2, [r3, #36]
	ldr	r3, .L1351+4
	ldr	r3, [r3, #28]
	add	r2, r2, r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L1322
	ldr	r3, [fp, #-24]
	orr	r3, r3, #47616
	orr	r3, r3, #207
	str	r3, [fp, #-24]
.L1322:
	ldr	r3, .L1351+8
	ldr	r3, [r3, #36]
	cmp	r3, #0
	blt	.L1323
	ldr	r3, .L1351+8
	ldr	r3, [r3, #28]
	cmp	r3, #0
	blt	.L1323
	ldr	r3, .L1351+8
	ldr	r2, [r3, #36]
	ldr	r3, .L1351+8
	ldr	r3, [r3, #28]
	add	r2, r2, r3
	ldr	r3, [fp, #-16]
	cmp	r2, r3
	blt	.L1323
	ldr	r3, [fp, #-24]
	orr	r3, r3, #46848
	orr	r3, r3, #214
	str	r3, [fp, #-24]
.L1323:
	ldr	r2, [fp, #-24]
	ldr	r3, .L1351+12
	cmp	r2, r3
	beq	.L1324
	ldr	r2, [fp, #-24]
	ldr	r3, .L1351+16
	cmp	r2, r3
	beq	.L1324
	mov	r3, #0
	str	r3, [fp, #-24]
.L1324:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L1325
.L1326:
	ldr	r2, [fp, #-20]
	ldr	r1, .L1351+4
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r2, [fp, #-20]
	ldr	r1, .L1351+8
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L1325:
	ldr	r3, [fp, #-20]
	cmp	r3, #4
	ble	.L1326
	ldr	r3, [fp, #-24]
	b	.L1327
.L1321:
	ldr	r3, [fp, #-36]
	cmp	r3, #1
	beq	.L1328
	ldr	r3, [fp, #-36]
	cmp	r3, #5
	ble	.L1329
.L1328:
	mov	r3, #0
	b	.L1327
.L1329:
	ldr	r0, [fp, #-32]
	ldr	r1, [fp, #-36]
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r0, [fp, #-28]
	ldr	r1, [fp, #-36]
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-44]
	ldr	r3, [fp, #-32]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	strh	r3, [fp, #-42]	@ movhi
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	mov	r3, r3, asl #20
	mov	r3, r3, lsr #20
	mov	r3, r3, asl #16
	mov	r2, r3, lsr #16
	ldr	r3, [fp, #-44]
	mov	r2, r2, asl #20
	mov	r3, r3, lsr #12
	orr	r3, r3, r2
	mov	r3, r3, ror #20
	str	r3, [fp, #-44]
	mov	r3, #0
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L1330
.L1333:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-40]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L1346
.L1331:
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-40]
	add	r2, r2, r3
	sub	r3, fp, #44
	mov	r0, r2
	mov	r1, r3
	mov	r2, #1
	bl	GestureDistance
	mov	r3, r0
	ldr	r2, [fp, #-24]
	add	r3, r2, r3
	str	r3, [fp, #-24]
	b	.L1332
.L1346:
	mov	r0, r0	@ nop
.L1332:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
.L1330:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	ble	.L1333
	ldr	r0, [fp, #-24]
	ldr	r1, [fp, #-36]
	bl	__aeabi_idiv
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-36]
	sub	r3, r3, #1
	str	r3, [fp, #-36]
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+4
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1334
	ldr	r3, [fp, #-36]
	ldr	r0, .L1351+4
	mov	r2, #4
	mov	r1, r3, asl #3
	add	r1, r0, r1
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	add	r2, r2, #1
	ldr	r0, .L1351+4
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r0, r3
	add	r3, r3, r1
	str	r2, [r3, #0]
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+4
	ldr	r1, [fp, #-24]
	str	r1, [r3, r2, asl #3]
	b	.L1335
.L1334:
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+4
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	blt	.L1347
.L1336:
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+4
	ldr	r2, [r3, r2, asl #3]
	ldr	r3, [fp, #-8]
	add	r2, r2, r3
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bge	.L1337
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+4
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mvn	r2, #0
	str	r2, [r3, #0]
	b	.L1335
.L1337:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-12]
	add	r2, r2, r3
	ldr	r1, [fp, #-36]
	ldr	r3, .L1351+4
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L1348
	ldr	r3, [fp, #-36]
	ldr	r0, .L1351+4
	mov	r2, #4
	mov	r1, r3, asl #3
	add	r1, r0, r1
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	add	r2, r2, #1
	ldr	r0, .L1351+4
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r0, r3
	add	r3, r3, r1
	str	r2, [r3, #0]
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+4
	ldr	r1, [fp, #-24]
	str	r1, [r3, r2, asl #3]
	b	.L1335
.L1347:
	mov	r0, r0	@ nop
	b	.L1335
.L1348:
	mov	r0, r0	@ nop
.L1335:
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+8
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L1339
	ldr	r3, [fp, #-36]
	ldr	r0, .L1351+8
	mov	r2, #4
	mov	r1, r3, asl #3
	add	r1, r0, r1
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	add	r2, r2, #1
	ldr	r0, .L1351+8
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r0, r3
	add	r3, r3, r1
	str	r2, [r3, #0]
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+8
	ldr	r1, [fp, #-24]
	str	r1, [r3, r2, asl #3]
	b	.L1340
.L1339:
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+8
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	cmp	r3, #0
	blt	.L1349
.L1341:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-8]
	add	r2, r2, r3
	ldr	r1, [fp, #-36]
	ldr	r3, .L1351+8
	ldr	r3, [r3, r1, asl #3]
	cmp	r2, r3
	bge	.L1342
	ldr	r2, [fp, #-36]
	ldr	r1, .L1351+8
	mov	r3, #4
	mov	r2, r2, asl #3
	add	r2, r1, r2
	add	r3, r2, r3
	mvn	r2, #0
	str	r2, [r3, #0]
	b	.L1340
.L1342:
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+8
	ldr	r2, [r3, r2, asl #3]
	ldr	r3, [fp, #-12]
	add	r2, r2, r3
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bge	.L1350
	ldr	r3, [fp, #-36]
	ldr	r0, .L1351+8
	mov	r2, #4
	mov	r1, r3, asl #3
	add	r1, r0, r1
	add	r2, r1, r2
	ldr	r2, [r2, #0]
	add	r2, r2, #1
	ldr	r0, .L1351+8
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r0, r3
	add	r3, r3, r1
	str	r2, [r3, #0]
	ldr	r2, [fp, #-36]
	ldr	r3, .L1351+8
	ldr	r1, [fp, #-24]
	str	r1, [r3, r2, asl #3]
	b	.L1340
.L1349:
	mov	r0, r0	@ nop
	b	.L1340
.L1350:
	mov	r0, r0	@ nop
.L1340:
	mov	r3, #0
.L1327:
	mov	r0, r3
	sub	sp, fp, #4
	ldmfd	sp!, {fp, lr}
	bx	lr
.L1352:
	.align	2
.L1351:
	.word	multi_set
	.word	multi_x
	.word	multi_o
	.word	47823
	.word	47062
	.size	GestureMulti, .-GestureMulti
	.align	2
	.global	gsl_GestureBuffer
	.type	gsl_GestureBuffer, %function
gsl_GestureBuffer:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #20
	str	r0, [fp, #-24]
	ldr	r3, .L1358
	ldr	r3, [r3, #0]
	cmp	r3, #255
	ble	.L1354
	ldr	r3, .L1358
	mov	r2, #255
	str	r2, [r3, #0]
.L1354:
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L1355
.L1356:
	ldr	r2, .L1358+4
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r4, r2, r3
	ldr	r2, .L1358+4
	ldr	r3, [fp, #-16]
	mov	r3, r3, asl #2
	add	r3, r2, r3
	mov	r0, r3
	bl	ScreenResolution
	mov	r3, r0
	str	r3, [r4, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
.L1355:
	ldr	r3, .L1358
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-16]
	cmp	r2, r3
	blt	.L1356
	ldr	r3, [fp, #-24]
	ldr	r2, .L1358+4
	str	r2, [r3, #0]
	ldr	r3, .L1358
	ldr	r3, [r3, #0]
	mov	r0, r3
	sub	sp, fp, #8
	ldmfd	sp!, {r4, fp, lr}
	bx	lr
.L1359:
	.align	2
.L1358:
	.word	gesture_num_last
	.word	config_static
	.size	gsl_GestureBuffer, .-gsl_GestureBuffer
	.section	.rodata
	.align	2
	.type	C.1276.7974, %object
	.size	C.1276.7974, 20
C.1276.7974:
	.word	-2
	.word	-1
	.word	0
	.word	1
	.word	2
	.local	save_dr.4569
	.comm	save_dr.4569, 40, 4
	.local	save_dn.4570
	.comm	save_dn.4570, 40, 4
	.ident	"GCC: (ctng-1.6.1) 4.4.3"
	.section	.note.GNU-stack, "", %progbits
