/*
 * TI LMU (Lighting Management Unit) Backlight Driver
 *
 * Copyright 2016 Texas Instruments
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-backlight.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/io.h>

#define NUM_DUAL_CHANNEL			2
#define LMU_BACKLIGHT_DUAL_CHANNEL_USED		(BIT(0) | BIT(1))
#define LMU_BACKLIGHT_11BIT_LSB_MASK		(BIT(0) | BIT(1) | BIT(2))
#define LMU_BACKLIGHT_11BIT_MSB_SHIFT		3
#define DEFAULT_PWM_NAME			"lmu-backlight"

static struct ti_lmu_bl_chip *bl_chip;

int translate_value[2048] = {0, 50, 55, 171, 253, 317, 370, 414, 452, 486, 516, 543, 568, 591, 612, 632, 651, 668, 684, 700, 714, 728, 742, 754, 767, 778, 790,\
												800, 811, 821, 831, 840, 849, 858, 866, 875, 883, 891, 898, 906, 913, 920, 927, 934, 940, 947, 953, 959, 965, 971, 977, 983, 988,\
												994, 999, 1004, 1009, 1014, 1019, 1024, 1029, 1034, 1038, 1043, 1048, 1052, 1056, 1061, 1065, 1069, 1073, 1077, 1081, 1085,\
												1089, 1093, 1097, 1101, 1104, 1108, 1111, 1115, 1119, 1122, 1125, 1129, 1132, 1135, 1139, 1142, 1145, 1148, 1151, 1155, 1158,\
												1161, 1164, 1167, 1170, 1172, 1175, 1178, 1181, 1184, 1187, 1189, 1192, 1195, 1197, 1200, 1203, 1205, 1208, 1210, 1213, 1215,\
												1218, 1220, 1223, 1225, 1228, 1230, 1232, 1235, 1237, 1239, 1242, 1244, 1246, 1248, 1251, 1253, 1255, 1257, 1259, 1261, 1263,\
												1266, 1268, 1270, 1272, 1274, 1276, 1278, 1280, 1282, 1284, 1286, 1288, 1290, 1291, 1293, 1295, 1297, 1299, 1301, 1303, 1305, 1306,\
												1308, 1310, 1312, 1314, 1315, 1317, 1319, 1321, 1322, 1324, 1326, 1327, 1329, 1331, 1332, 1334, 1336, 1337, 1339, 1341, 1342,\
												1344, 1345, 1347, 1348, 1350, 1352, 1353, 1355, 1356, 1358, 1359, 1361, 1362, 1364, 1365, 1367, 1368, 1370, 1371, 1372, 1374,\
												1375, 1377, 1378, 1380, 1381, 1382, 1384, 1385, 1386, 1388, 1389, 1391, 1392, 1393, 1395, 1396, 1397, 1399, 1400, 1401, 1402,\
												1404, 1405, 1406, 1408, 1409, 1410, 1411, 1413, 1414, 1415, 1416, 1418, 1419, 1420, 1421, 1423, 1424, 1425, 1426, 1427, 1428,\
												1430, 1431, 1432, 1433, 1434, 1435, 1437, 1438, 1439, 1440, 1441, 1442, 1443, 1445, 1446, 1447, 1448, 1449, 1450, 1451, 1452,\
												1453, 1454, 1456, 1457, 1458, 1459, 1460, 1461, 1462, 1463, 1464, 1465, 1466, 1467, 1468, 1469, 1470, 1471, 1472, 1473, 1474,\
												1475, 1476, 1477, 1478, 1479, 1480, 1481, 1482, 1483, 1484, 1485, 1486, 1487, 1488, 1489, 1490, 1491, 1492, 1493, 1494, 1495,\
												1496, 1497, 1498, 1498, 1499, 1500, 1501, 1502, 1503, 1504, 1505, 1506, 1507, 1508, 1508, 1509, 1510, 1511, 1512, 1513, 1514,\
												1515, 1516, 1516, 1517, 1518, 1519, 1520, 1521, 1522, 1522, 1523, 1524, 1525, 1526, 1527, 1528, 1528, 1529, 1530, 1531, 1532,\
												1533, 1533, 1534, 1535, 1536, 1537, 1537, 1538, 1539, 1540, 1541, 1541, 1542, 1543, 1544, 1545, 1545, 1546, 1547, 1548, 1549,\
												1549, 1550, 1551, 1552, 1552, 1553, 1554, 1555, 1555, 1556, 1557, 1558, 1558, 1559, 1560, 1561, 1561, 1562, 1563, 1564, 1564,\
												1565, 1566, 1567, 1567, 1568, 1569, 1570, 1570, 1571, 1572, 1572, 1573, 1574, 1575, 1575, 1576, 1577, 1577, 1578, 1579, 1579,\
												1580, 1581, 1582, 1582, 1583, 1584, 1584, 1585, 1586, 1586, 1587, 1588, 1588, 1589, 1590, 1590, 1591, 1592, 1592, 1593, 1594,\
												1594, 1595, 1596, 1596, 1597, 1598, 1598, 1599, 1600, 1600, 1601, 1602, 1602, 1603, 1604, 1604, 1605, 1606, 1606, 1607, 1607,\
												1608, 1609, 1609, 1610, 1611, 1611, 1612, 1612, 1613, 1614, 1614, 1615, 1616, 1616, 1617, 1617, 1618, 1619, 1619, 1620, 1620,\
												1621, 1622, 1622, 1623, 1623, 1624, 1625, 1625, 1626, 1626, 1627, 1628, 1628, 1629, 1629, 1630, 1631, 1631, 1632, 1632, 1633,\
												1633, 1634, 1635, 1635, 1636, 1636, 1637, 1637, 1638, 1639, 1639, 1640, 1640, 1641, 1641, 1642, 1643, 1643, 1644, 1644, 1645,\
												1645, 1646, 1646, 1647, 1648, 1648, 1649, 1649, 1650, 1650, 1651, 1651, 1652, 1652, 1653, 1654, 1654, 1655, 1655, 1656, 1656,\
												1657, 1657, 1658, 1658, 1659, 1659, 1660, 1660, 1661, 1662, 1662, 1663, 1663, 1664, 1664, 1665, 1665, 1666, 1666, 1667, 1667,\
												1668, 1668, 1669, 1669, 1670, 1670, 1671, 1671, 1672, 1672, 1673, 1673, 1674, 1674, 1675, 1675, 1676, 1676, 1677, 1677, 1678,\
												1678, 1679, 1679, 1680, 1680, 1681, 1681, 1682, 1682, 1683, 1683, 1684, 1684, 1685, 1685, 1686, 1686, 1687, 1687, 1688, 1688,\
												1689, 1689, 1689, 1690, 1690, 1691, 1691, 1692, 1692, 1693, 1693, 1694, 1694, 1695, 1695, 1696, 1696, 1697, 1697, 1697, 1698,\
												1698, 1699, 1699, 1700, 1700, 1701, 1701, 1702, 1702, 1703, 1703, 1703, 1704, 1704, 1705, 1705, 1706, 1706, 1707, 1707, 1707,\
												1708, 1708, 1709, 1709, 1710, 1710, 1711, 1711, 1711, 1712, 1712, 1713, 1713, 1714, 1714, 1715, 1715, 1715, 1716, 1716, 1717,\
												1717, 1718, 1718, 1718, 1719, 1719, 1720, 1720, 1721, 1721, 1721, 1722, 1722, 1723, 1723, 1724, 1724, 1724, 1725, 1725, 1726,\
												1726, 1726, 1727, 1727, 1728, 1728, 1729, 1729, 1729, 1730, 1730, 1731, 1731, 1731, 1732, 1732, 1733, 1733, 1733, 1734, 1734,\
												1735, 1735, 1736, 1736, 1736, 1737, 1737, 1738, 1738, 1738, 1739, 1739, 1740, 1740, 1740, 1741, 1741, 1742, 1742, 1742, 1743,\
												1743, 1744, 1744, 1744, 1745, 1745, 1745, 1746, 1746, 1747, 1747, 1747, 1748, 1748, 1749, 1749, 1749, 1750, 1750, 1751, 1751,\
												1751, 1752, 1752, 1752, 1753, 1753, 1754, 1754, 1754, 1755, 1755, 1755, 1756, 1756, 1757, 1757, 1757, 1758, 1758, 1758, 1759,\
												1759, 1760, 1760, 1760, 1761, 1761, 1761, 1762, 1762, 1763, 1763, 1763, 1764, 1764, 1764, 1765, 1765, 1766, 1766, 1766, 1767,\
												1767, 1767, 1768, 1768, 1768, 1769, 1769, 1769, 1770, 1770, 1771, 1771, 1771, 1772, 1772, 1772, 1773, 1773, 1773, 1774, 1774,\
												1774, 1775, 1775, 1776, 1776, 1776, 1777, 1777, 1777, 1778, 1778, 1778, 1779, 1779, 1779, 1780, 1780, 1780, 1781, 1781, 1781,\
												1782, 1782, 1782, 1783, 1783, 1784, 1784, 1784, 1785, 1785, 1785, 1786, 1786, 1786, 1787, 1787, 1787, 1788, 1788, 1788, 1789,\
												1789, 1789, 1790, 1790, 1790, 1791, 1791, 1791, 1792, 1792, 1792, 1793, 1793, 1793, 1794, 1794, 1794, 1795, 1795, 1795, 1796,\
												1796, 1796, 1797, 1797, 1797, 1798, 1798, 1798, 1799, 1799, 1799, 1800, 1800, 1800, 1800, 1801, 1801, 1801, 1802, 1802, 1802,\
												1803, 1803, 1803, 1804, 1804, 1804, 1805, 1805, 1805, 1806, 1806, 1806, 1807, 1807, 1807, 1808, 1808, 1808, 1808, 1809, 1809,\
												1809, 1810, 1810, 1810, 1811, 1811, 1811, 1812, 1812, 1812, 1813, 1813, 1813, 1813, 1814, 1814, 1814, 1815, 1815, 1815, 1816,\
												1816, 1816, 1816, 1817, 1817, 1817, 1818, 1818, 1818, 1819, 1819, 1819, 1820, 1820, 1820, 1820, 1821, 1821, 1821, 1822, 1822,\
												1822, 1823, 1823, 1823, 1823, 1824, 1824, 1824, 1825, 1825, 1825, 1826, 1826, 1826, 1826, 1827, 1827, 1827, 1828, 1828, 1828,\
												1828, 1829, 1829, 1829, 1830, 1830, 1830, 1831, 1831, 1831, 1831, 1832, 1832, 1832, 1833, 1833, 1833, 1833, 1834, 1834, 1834,\
												1835, 1835, 1835, 1835, 1836, 1836, 1836, 1837, 1837, 1837, 1837, 1838, 1838, 1838, 1839, 1839, 1839, 1839, 1840, 1840, 1840,\
												1841, 1841, 1841, 1841, 1842, 1842, 1842, 1842, 1843, 1843, 1843, 1844, 1844, 1844, 1844, 1845, 1845, 1845, 1846, 1846, 1846,\
												1846, 1847, 1847, 1847, 1847, 1848, 1848, 1848, 1849, 1849, 1849, 1849, 1850, 1850, 1850, 1850, 1851, 1851, 1851, 1852, 1852,\
												1852, 1852, 1853, 1853, 1853, 1853, 1854, 1854, 1854, 1854, 1855, 1855, 1855, 1856, 1856, 1856, 1856, 1857, 1857, 1857, 1857,\
												1858, 1858, 1858, 1858, 1859, 1859, 1859, 1860, 1860, 1860, 1860, 1861, 1861, 1861, 1861, 1862, 1862, 1862, 1862, 1863, 1863,\
												1863, 1863, 1864, 1864, 1864, 1864, 1865, 1865, 1865, 1865, 1866, 1866, 1866, 1867, 1867, 1867, 1867, 1868, 1868, 1868, 1868,\
												1869, 1869, 1869, 1869, 1870, 1870, 1870, 1870, 1871, 1871, 1871, 1871, 1872, 1872, 1872, 1872, 1873, 1873, 1873, 1873, 1874,\
												1874, 1874, 1874, 1875, 1875, 1875, 1875, 1876, 1876, 1876, 1876, 1877, 1877, 1877, 1877, 1878, 1878, 1878, 1878, 1879, 1879,\
												1879, 1879, 1880, 1880, 1880, 1880, 1881, 1881, 1881, 1881, 1882, 1882, 1882, 1882, 1882, 1883, 1883, 1883, 1883, 1884, 1884,\
												1884, 1884, 1885, 1885, 1885, 1885, 1886, 1886, 1886, 1886, 1887, 1887, 1887, 1887, 1888, 1888, 1888, 1888, 1888, 1889, 1889,\
												1889, 1889, 1890, 1890, 1890, 1890, 1891, 1891, 1891, 1891, 1892, 1892, 1892, 1892, 1892, 1893, 1893, 1893, 1893, 1894, 1894,\
												1894, 1894, 1895, 1895, 1895, 1895, 1896, 1896, 1896, 1896, 1896, 1897, 1897, 1897, 1897, 1898, 1898, 1898, 1898, 1899, 1899,\
												1899, 1899, 1899, 1900, 1900, 1900, 1900, 1901, 1901, 1901, 1901, 1901, 1902, 1902, 1902, 1902, 1903, 1903, 1903, 1903, 1904,\
												1904, 1904, 1904, 1904, 1905, 1905, 1905, 1905, 1906, 1906, 1906, 1906, 1906, 1907, 1907, 1907, 1907, 1908, 1908, 1908, 1908,\
												1908, 1909, 1909, 1909, 1909, 1910, 1910, 1910, 1910, 1910, 1911, 1911, 1911, 1911, 1912, 1912, 1912, 1912, 1912, 1913, 1913,\
												1913, 1913, 1913, 1914, 1914, 1914, 1914, 1915, 1915, 1915, 1915, 1915, 1916, 1916, 1916, 1916, 1917, 1917, 1917, 1917, 1917,\
												1918, 1918, 1918, 1918, 1918, 1919, 1919, 1919, 1919, 1920, 1920, 1920, 1920, 1920, 1921, 1921, 1921, 1921, 1921, 1922, 1922,\
												1922, 1922, 1922, 1923, 1923, 1923, 1923, 1924, 1924, 1924, 1924, 1924, 1925, 1925, 1925, 1925, 1925, 1926, 1926, 1926, 1926,\
												1926, 1927, 1927, 1927, 1927, 1927, 1928, 1928, 1928, 1928, 1929, 1929, 1929, 1929, 1929, 1930, 1930, 1930, 1930, 1930, 1931,\
												1931, 1931, 1931, 1931, 1932, 1932, 1932, 1932, 1932, 1933, 1933, 1933, 1933, 1933, 1934, 1934, 1934, 1934, 1934, 1935, 1935,\
												1935, 1935, 1935, 1936, 1936, 1936, 1936, 1936, 1937, 1937, 1937, 1937, 1937, 1938, 1938, 1938, 1938, 1938, 1939, 1939, 1939,\
												1939, 1939, 1940, 1940, 1940, 1940, 1940, 1941, 1941, 1941, 1941, 1941, 1942, 1942, 1942, 1942, 1942, 1943, 1943, 1943, 1943,\
												1943, 1944, 1944, 1944, 1944, 1944, 1945, 1945, 1945, 1945, 1945, 1946, 1946, 1946, 1946, 1946, 1947, 1947, 1947, 1947, 1947,\
												1947, 1948, 1948, 1948, 1948, 1948, 1949, 1949, 1949, 1949, 1949, 1950, 1950, 1950, 1950, 1950, 1951, 1951, 1951, 1951, 1951,\
												1952, 1952, 1952, 1952, 1952, 1952, 1953, 1953, 1953, 1953, 1953, 1954, 1954, 1954, 1954, 1954, 1955, 1955, 1955, 1955, 1955,\
												1956, 1956, 1956, 1956, 1956, 1956, 1957, 1957, 1957, 1957, 1957, 1958, 1958, 1958, 1958, 1958, 1958, 1959, 1959, 1959, 1959,\
												1959, 1960, 1960, 1960, 1960, 1960, 1961, 1961, 1961, 1961, 1961, 1961, 1962, 1962, 1962, 1962, 1962, 1963, 1963, 1963, 1963,\
												1963, 1963, 1964, 1964, 1964, 1964, 1964, 1965, 1965, 1965, 1965, 1965, 1965, 1966, 1966, 1966, 1966, 1966, 1967, 1967, 1967,\
												1967, 1967, 1967, 1968, 1968, 1968, 1968, 1968, 1969, 1969, 1969, 1969, 1969, 1969, 1970, 1970, 1970, 1970, 1970, 1971, 1971,\
												1971, 1971, 1971, 1971, 1972, 1972, 1972, 1972, 1972, 1972, 1973, 1973, 1973, 1973, 1973, 1974, 1974, 1974, 1974, 1974, 1974,\
												1975, 1975, 1975, 1975, 1975, 1975, 1976, 1976, 1976, 1976, 1976, 1977, 1977, 1977, 1977, 1977, 1977, 1978, 1978, 1978, 1978,\
												1978, 1978, 1979, 1979, 1979, 1979, 1979, 1979, 1980, 1980, 1980, 1980, 1980, 1980, 1981, 1981, 1981, 1981, 1981, 1982, 1982,\
												1982, 1982, 1982, 1982, 1983, 1983, 1983, 1983, 1983, 1983, 1984, 1984, 1984, 1984, 1984, 1984, 1985, 1985, 1985, 1985, 1985,\
												1985, 1986, 1986, 1986, 1986, 1986, 1986, 1987, 1987, 1987, 1987, 1987, 1987, 1988, 1988, 1988, 1988, 1988, 1988, 1989, 1989,\
												1989, 1989, 1989, 1989, 1990, 1990, 1990, 1990, 1990, 1990, 1991, 1991, 1991, 1991, 1991, 1991, 1992, 1992, 1992, 1992, 1992,\
												1992, 1993, 1993, 1993, 1993, 1993, 1993, 1994, 1994, 1994, 1994, 1994, 1994, 1995, 1995, 1995, 1995, 1995, 1995, 1996, 1996,\
												1996, 1996, 1996, 1996, 1997, 1997, 1997, 1997, 1997, 1997, 1998, 1998, 1998, 1998, 1998, 1998, 1999, 1999, 1999, 1999, 1999,\
												1999, 1999, 2000, 2000, 2000, 2000, 2000, 2000, 2001, 2001, 2001, 2001, 2001, 2001, 2002, 2002, 2002, 2002, 2002, 2002, 2003,\
												2003, 2003, 2003, 2003, 2003, 2003, 2004, 2004, 2004, 2004, 2004, 2004, 2005, 2005, 2005, 2005, 2005, 2005, 2006, 2006, 2006,\
												2006, 2006, 2006, 2006, 2007, 2007, 2007, 2007, 2007, 2007, 2008, 2008, 2008, 2008, 2008, 2008, 2009, 2009, 2009, 2009, 2009,\
												2009, 2009, 2010, 2010, 2010, 2010, 2010, 2010, 2011, 2011, 2011, 2011, 2011, 2011, 2011, 2012, 2012, 2012, 2012, 2012, 2012,\
												2013, 2013, 2013, 2013, 2013, 2013, 2013, 2014, 2014, 2014, 2014, 2014, 2014, 2015, 2015, 2015, 2015, 2015, 2015, 2015, 2016,\
												2016, 2016, 2016, 2016, 2016, 2017, 2017, 2017, 2017, 2017, 2017, 2017, 2018, 2018, 2018, 2018, 2018, 2018, 2019, 2019, 2019,\
												2019, 2019, 2019, 2019, 2020, 2020, 2020, 2020, 2020, 2020, 2020, 2021, 2021, 2021, 2021, 2021, 2021, 2022, 2022, 2022, 2022,\
												2022, 2022, 2022, 2023, 2023, 2023, 2023, 2023, 2023, 2023, 2024, 2024, 2024, 2024, 2024, 2024, 2024, 2025, 2025, 2025, 2025,\
												2025, 2025, 2026, 2026, 2026, 2026, 2026, 2026, 2026, 2027, 2027, 2027, 2027, 2027, 2027, 2027, 2028, 2028, 2028, 2028, 2028,\
												2028, 2028, 2029, 2029, 2029, 2029, 2029, 2029, 2029, 2030, 2030, 2030, 2030, 2030, 2030, 2030, 2031, 2031, 2031, 2031, 2031,\
												2031, 2031, 2032, 2032, 2032, 2032, 2032, 2032, 2032, 2033, 2033, 2033, 2033, 2033, 2033, 2033, 2034, 2034, 2034, 2034, 2034,\
												2034, 2034, 2035, 2035, 2035, 2035, 2035, 2035, 2035, 2036, 2036, 2036, 2036, 2036, 2036, 2036, 2037, 2037, 2037, 2037, 2037,\
												2037, 2037, 2038, 2038, 2038, 2038, 2038, 2038, 2038, 2039, 2039, 2039, 2039, 2039, 2039, 2039, 2040, 2040, 2040, 2040};

static int dump_i2c_reg(struct ti_lmu_bl_chip *chip)
{
	struct regmap *regmap = chip->lmu->regmap;
	int ret_val = 0;
#if 1
	regmap_read(regmap, 0x10, &ret_val);
	pr_err("[bkl] %s read 0x10 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x1A, &ret_val);
	pr_err("[bkl] %s read 0x1A = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x1C, &ret_val);
	pr_err("[bkl] %s read 0x1C = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x22, &ret_val);
	pr_err("[bkl] %s read 0x22 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x23, &ret_val);
	pr_err("[bkl] %s read 0x23 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x24, &ret_val);
	pr_err("[bkl] %s read 0x24 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x18, &ret_val);
	pr_err("[bkl] %s read 0x18 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x16, &ret_val);
	pr_err("[bkl] %s read 0x16 = 0x%x\n", __func__, ret_val);
#else
	regmap_read(regmap, 0x00, &ret_val);
	pr_err("[bkl] %s read 0x00 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x01, &ret_val);
	pr_err("[bkl] %s read 0x01 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x10, &ret_val);
	pr_err("[bkl] %s read 0x10 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x11, &ret_val);
	pr_err("[bkl] %s read 0x11 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x12, &ret_val);
	pr_err("[bkl] %s read 0x12 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x13, &ret_val);
	pr_err("[bkl] %s read 0x13 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x14, &ret_val);
	pr_err("[bkl] %s read 0x14 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x15, &ret_val);
	pr_err("[bkl] %s read 0x15 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x16, &ret_val);
	pr_err("[bkl] %s read 0x16 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x17, &ret_val);
	pr_err("[bkl] %s read 0x17 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x18, &ret_val);
	pr_err("[bkl] %s read 0x18 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x19, &ret_val);
	pr_err("[bkl] %s read 0x19 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x1A, &ret_val);
	pr_err("[bkl] %s read 0x1A = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x1B, &ret_val);
	pr_err("[bkl] %s read 0x1B = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x1C, &ret_val);
	pr_err("[bkl] %s read 0x1C = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x20, &ret_val);
	pr_err("[bkl] %s read 0x20 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x21, &ret_val);
	pr_err("[bkl] %s read 0x21 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x22, &ret_val);
	pr_err("[bkl] %s read 0x22 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x23, &ret_val);
	pr_err("[bkl] %s read 0x23 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0x24, &ret_val);
	pr_err("[bkl] %s read 0x24 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0xB0, &ret_val);
	pr_err("[bkl] %s read 0xB0 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0xB2, &ret_val);
	pr_err("[bkl] %s read 0xB2 = 0x%x\n", __func__, ret_val);
	regmap_read(regmap, 0xB4, &ret_val);
	pr_err("[bkl] %s read 0xB4 = 0x%x\n", __func__, ret_val);
#endif

	return 0;
}

int ti_hbm_set(enum backlight_hbm_mode hbm_mode)
{
	struct regmap *regmap = bl_chip->lmu->regmap;
	int value = 0;

	pr_err("[bkl] %s enter\n", __func__);

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		regmap_write(regmap, 0x18, 0x13);
		pr_err("This is hbm mode 1\n");
		break;
	case HBM_MODE_LEVEL1:
		regmap_write(regmap, 0x18, 0x16);
		pr_err("This is hbm mode 2\n");
		break;
	case HBM_MODE_LEVEL2:
		regmap_write(regmap, 0x18, 0x19);
		pr_err("This is hbm mode 3\n");
		break;
	default:
		pr_err("This isn't hbm mode\n");
		break;
	}

	regmap_read(regmap, 0x18, &value);
	pr_err("[bkl]%s hbm_mode = %d,regmap value=0x%x\n", __func__, hbm_mode, value);
	return 0;
}

static int ti_lmu_backlight_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	struct ti_lmu_bl_chip *chip = lmu_bl->chip;
	struct regmap *regmap = chip->lmu->regmap;
	unsigned long enable_time = chip->cfg->reginfo->enable_usec;
	u8 *reg = chip->cfg->reginfo->enable;
	//u8 offset = chip->cfg->reginfo->enable_offset;
	//u8 mask = BIT(lmu_bl->bank_id) << offset;

	if (!reg)
		return -EINVAL;

	if (enable)
		return regmap_write(regmap, *reg, 0x02);
	else
		return regmap_write(regmap, *reg, 0x00);

	if (enable_time > 0)
		usleep_range(enable_time, enable_time + 100);
}

static void ti_lmu_backlight_pwm_ctrl(struct ti_lmu_bl *lmu_bl, int brightness,
				      int max_brightness)
{
	struct pwm_device *pwm;
	unsigned int duty, period;

	if (!lmu_bl->pwm) {
		pwm = devm_pwm_get(lmu_bl->chip->dev, DEFAULT_PWM_NAME);
		if (IS_ERR(pwm)) {
			dev_err(lmu_bl->chip->dev,
				"Can not get PWM device, err: %ld\n",
				PTR_ERR(pwm));
			return;
		}

		lmu_bl->pwm = pwm;
	}

	period = lmu_bl->pwm_period;
	duty = brightness * period / max_brightness;

	pwm_config(lmu_bl->pwm, duty, period);
	if (duty)
		pwm_enable(lmu_bl->pwm);
	else
		pwm_disable(lmu_bl->pwm);
}

static int ti_lmu_backlight_update_brightness_register(struct ti_lmu_bl *lmu_bl,
						       int brightness)
{
	const struct ti_lmu_bl_cfg *cfg = lmu_bl->chip->cfg;
	const struct ti_lmu_bl_reg *reginfo = cfg->reginfo;
	struct regmap *regmap = lmu_bl->chip->lmu->regmap;
	u8 reg, val;
	int ret;
	int i = 0;

	regmap_write(regmap, 0x13, 0x01);

	if (lmu_bl->mode == BL_PWM_BASED) {
		switch (cfg->pwm_action) {
		case UPDATE_PWM_ONLY:
			/* No register update is required */
			return 0;
		case UPDATE_MAX_BRT:
			/*
			 * PWM can start from any non-zero code and dim down
			 * to zero. So, brightness register should be updated
			 * even in PWM mode.
			 */
			if (brightness > 0)
				brightness = MAX_BRIGHTNESS_11BIT;
			else
				brightness = 0;
			break;
		default:
			break;
		}
	}

	/*
	 * Brightness register update
	 *
	 * 11 bit dimming: update LSB bits and write MSB byte.
	 *		   MSB brightness should be shifted.
	 *  8 bit dimming: write MSB byte.
	 */
	if (brightness < 0) {
		return -EINVAL;
	}

	if (!reginfo->brightness_msb)
		return -EINVAL;

	if (cfg->max_brightness == MAX_BRIGHTNESS_11BIT) {
		if (!reginfo->brightness_lsb)
			return -EINVAL;

		i = brightness;
		brightness = translate_value[i];

		reg = reginfo->brightness_lsb[lmu_bl->bank_id];
		ret = regmap_update_bits(regmap, reg,
					 LMU_BACKLIGHT_11BIT_LSB_MASK,
					 brightness);
		if (ret)
			return ret;
		pr_err("[bkl][after]11bit %s brightness = %d\n", __func__, brightness);
		val = (brightness >> LMU_BACKLIGHT_11BIT_MSB_SHIFT) & 0xFF;
	} else {
		val = brightness & 0xFF;
		pr_err("[bkl]8bit %s val = %d\n", __func__, val);
	}

	reg = reginfo->brightness_msb[lmu_bl->bank_id];
	return regmap_write(regmap, reg, val);
}

int lm3697_set_brightness(int brightness)
{
	pr_err("[bkl][before]%s brightness = %d\n", __func__, brightness);
	return ti_lmu_backlight_update_brightness_register(bl_chip->lmu_bl, brightness);
}

static int ti_lmu_backlight_update_status(struct backlight_device *bl_dev)
{
	struct ti_lmu_bl *lmu_bl = bl_get_data(bl_dev);
	int brightness = bl_dev->props.brightness;
	int ret;

	if (bl_dev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	if (brightness > 0)
		ret = ti_lmu_backlight_enable(lmu_bl, 1);
	else
		ret = ti_lmu_backlight_enable(lmu_bl, 0);

	if (ret)
		return ret;

	if (lmu_bl->mode == BL_PWM_BASED)
		ti_lmu_backlight_pwm_ctrl(lmu_bl, brightness,
					  bl_dev->props.max_brightness);

	return ti_lmu_backlight_update_brightness_register(lmu_bl, brightness);
}

static const struct backlight_ops lmu_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ti_lmu_backlight_update_status,
};

#if 0
static int of_property_count_elems_of_size(const struct device_node *np,
				const char *propname, int elem_size)
{
	struct property *prop = of_find_property(np, propname, NULL);

	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	if (prop->length % elem_size != 0) {
		pr_err("size of %s in node %s is not a multiple of %d\n",
		       propname, np->full_name, elem_size);
		return -EINVAL;
	}

	return prop->length / elem_size;
}

/**
 * of_property_count_u32_elems - Count the number of u32 elements in a property
 *
 * @np:		device node from which the property value is to be read.
 * @propname:	name of the property to be searched.
 *
 * Search for a property in a device node and count the number of u32 elements
 * in it. Returns number of elements on sucess, -EINVAL if the property does
 * not exist or its length does not match a multiple of u32 and -ENODATA if the
 * property does not have a value.
 */
static int of_property_count_u32_elems(const struct device_node *np,
				const char *propname)
{
	return of_property_count_elems_of_size(np, propname, sizeof(u32));
}
#endif

static int ti_lmu_backlight_of_get_ctrl_bank(struct device_node *np,
					     struct ti_lmu_bl *lmu_bl)
{
	const char *name;
	u32 *sources;
	int num_channels = lmu_bl->chip->cfg->num_channels;
	int ret, num_sources;

	sources = devm_kzalloc(lmu_bl->chip->dev, num_channels, GFP_KERNEL);
	if (!sources)
		return -ENOMEM;

	if (!of_property_read_string(np, "label", &name))
		lmu_bl->name = name;
	else
		lmu_bl->name = np->name;

	ret = of_property_count_u32_elems(np, "led-sources");
	if (ret < 0 || ret > num_channels)
		return -EINVAL;

	num_sources = ret;
	ret = of_property_read_u32_array(np, "led-sources", sources,
					 num_sources);
	if (ret)
		return ret;

	lmu_bl->led_sources = 0;
	while (num_sources--)
		set_bit(sources[num_sources], &lmu_bl->led_sources);

	return 0;
}

static void ti_lmu_backlight_of_get_light_properties(struct device_node *np,
						     struct ti_lmu_bl *lmu_bl)
{
	of_property_read_u32(np, "default-brightness-level",
			     &lmu_bl->default_brightness);

	of_property_read_u32(np, "ramp-up-msec",  &lmu_bl->ramp_up_msec);
	of_property_read_u32(np, "ramp-down-msec", &lmu_bl->ramp_down_msec);
}

static void ti_lmu_backlight_of_get_brightness_mode(struct device_node *np,
						    struct ti_lmu_bl *lmu_bl)
{
	of_property_read_u32(np, "pwm-period", &lmu_bl->pwm_period);

	if (lmu_bl->pwm_period > 0)
		lmu_bl->mode = BL_PWM_BASED;
	else
		lmu_bl->mode = BL_REGISTER_BASED;
}

static int ti_lmu_backlight_of_create(struct ti_lmu_bl_chip *chip,
				      struct device_node *np)
{
	struct device_node *child;
	struct ti_lmu_bl *lmu_bl, *each;
	int ret, num_backlights;
	int i = 0;

	num_backlights = of_get_child_count(np);
	if (num_backlights == 0) {
		dev_err(chip->dev, "No backlight strings\n");
		return -ENODEV;
	}

	/* One chip can have mulitple backlight strings */
	lmu_bl = devm_kzalloc(chip->dev, sizeof(*lmu_bl) * num_backlights,
			      GFP_KERNEL);
	if (!lmu_bl)
		return -ENOMEM;

	/* Child is mapped to LMU backlight control bank */
	for_each_child_of_node(np, child) {
		each = lmu_bl + i;
		each->bank_id = i;
		each->chip = chip;

		ret = ti_lmu_backlight_of_get_ctrl_bank(child, each);
		if (ret) {
			of_node_put(np);
			return ret;
		}

		ti_lmu_backlight_of_get_light_properties(child, each);
		ti_lmu_backlight_of_get_brightness_mode(child, each);

		i++;
	}

	chip->lmu_bl = lmu_bl;
	chip->num_backlights = num_backlights;

	return 0;
}

#if 0
static int ti_lmu_backlight_create_channel(struct ti_lmu_bl *lmu_bl)
{
	struct regmap *regmap = lmu_bl->chip->lmu->regmap;
	u32 *reg = lmu_bl->chip->cfg->reginfo->channel;
	int num_channels = lmu_bl->chip->cfg->num_channels;
	int i, ret;
	u8 val;

	/*
	 * How to create backlight output channels:
	 *   Check 'led_sources' bit and update registers.
	 *
	 *   1) Dual channel configuration
	 *     The 1st register data is used for single channel.
	 *     The 2nd register data is used for dual channel.
	 *
	 *   2) Multiple channel configuration
	 *     Each register data is mapped to bank ID.
	 *     Bit shift operation is defined in channel registers.
	 *
	 * Channel register data consists of address, mask, value.
	 * Driver can get each data by using LMU_BL_GET_ADDR(),
	 * LMU_BL_GET_MASK(), LMU_BL_GET_VAL().
	 */

	if (num_channels == NUM_DUAL_CHANNEL) {
		if (lmu_bl->led_sources == LMU_BACKLIGHT_DUAL_CHANNEL_USED)
			++reg;

		return regmap_update_bits(regmap, LMU_BL_GET_ADDR(*reg),
					  LMU_BL_GET_MASK(*reg),
					  LMU_BL_GET_VAL(*reg));
	}

	for (i = 0; i < num_channels; i++) {
		if (!reg)
			break;

		/*
		 * The result of LMU_BL_GET_VAL()
		 *
		 * Register set value. One bank controls multiple channels.
		 * LM36274 data should be configured with 'single_bank_used'.
		 * Otherwise, the result is shift bit.
		 * The bank_id should be shifted for the channel configuration.
		 */

		if (test_bit(i, &lmu_bl->led_sources)) {
			if (lmu_bl->chip->cfg->single_bank_used)
				val = LMU_BL_GET_VAL(*reg);
			else
				val = lmu_bl->bank_id << LMU_BL_GET_VAL(*reg);

			ret = regmap_update_bits(regmap, LMU_BL_GET_ADDR(*reg),
						 LMU_BL_GET_MASK(*reg), val);
			if (ret)
				return ret;
		}

		reg++;
	}

	return 0;
}
#endif

static int ti_lmu_backlight_update_ctrl_mode(struct ti_lmu_bl *lmu_bl)
{
	struct regmap *regmap = lmu_bl->chip->lmu->regmap;
	u32 *reg = lmu_bl->chip->cfg->reginfo->mode + lmu_bl->bank_id;
	u8 val;

	if (!reg)
		return 0;

	/*
	 * Update PWM configuration register.
	 * If the mode is register based, then clear the bit.
	 */
	pr_err("[bkl] %s reg = 0x%x\n", __func__, *reg);
	if (lmu_bl->mode == BL_PWM_BASED)
		val = LMU_BL_GET_VAL(*reg);
	else
		val = 0;

	return regmap_update_bits(regmap, LMU_BL_GET_ADDR(*reg),
				  LMU_BL_GET_MASK(*reg), val);
}

static int ti_lmu_backlight_convert_ramp_to_index(struct ti_lmu_bl *lmu_bl,
						  enum ti_lmu_bl_ramp_mode mode)
{
	const int *ramp_table = lmu_bl->chip->cfg->ramp_table;
	const int size = lmu_bl->chip->cfg->size_ramp;
	unsigned int msec;
	int i;

	if (!ramp_table)
		return -EINVAL;

	switch (mode) {
	case BL_RAMP_UP:
		msec = lmu_bl->ramp_up_msec;
		break;
	case BL_RAMP_DOWN:
		msec = lmu_bl->ramp_down_msec;
		break;
	default:
		return -EINVAL;
	}

	if (msec <= ramp_table[0])
		return 0;

	if (msec > ramp_table[size - 1])
		return size - 1;

	for (i = 1; i < size; i++) {
		if (msec == ramp_table[i])
			return i;

		/* Find an approximate index by looking up the table */
		if (msec > ramp_table[i - 1] && msec < ramp_table[i]) {
			if (msec - ramp_table[i - 1] < ramp_table[i] - msec)
				return i - 1;
			else
				return i;
		}
	}

	return -EINVAL;
}

static int ti_lmu_backlight_set_ramp(struct ti_lmu_bl *lmu_bl)
{
	struct regmap *regmap = lmu_bl->chip->lmu->regmap;
	const struct ti_lmu_bl_reg *reginfo = lmu_bl->chip->cfg->reginfo;
	int offset = reginfo->ramp_reg_offset;
	int i, ret, index;
	u32 reg;

	for (i = BL_RAMP_UP; i <= BL_RAMP_DOWN; i++) {
		index = ti_lmu_backlight_convert_ramp_to_index(lmu_bl, i);
		if (index > 0) {
			if (!reginfo->ramp)
				break;

			if (lmu_bl->bank_id == 0)
				reg = reginfo->ramp[i];
			else
				reg = reginfo->ramp[i] + offset;

			/*
			 * Note that the result of LMU_BL_GET_VAL() is
			 * shift bit. So updated bit is shifted index value.
			 */
			ret = regmap_update_bits(regmap, LMU_BL_GET_ADDR(reg),
						 LMU_BL_GET_MASK(reg),
						 index << LMU_BL_GET_VAL(reg));
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int ti_lmu_backlight_configure(struct ti_lmu_bl *lmu_bl)
{
	int ret;

	//ret = ti_lmu_backlight_create_channel(lmu_bl);
	//if (ret)
	//	return ret;

	ret = ti_lmu_backlight_update_ctrl_mode(lmu_bl);
	if (ret)
		return ret;

	return ti_lmu_backlight_set_ramp(lmu_bl);
}

static int ti_lmu_backlight_init(struct ti_lmu_bl_chip *chip)
{
	struct regmap *regmap = chip->lmu->regmap;
	//u32 *reg = chip->cfg->reginfo->init;
	//int num_init = chip->cfg->reginfo->num_init;
	//int i, ret;
	pr_err("[bkl] %s enter\n", __func__);
	/*
	 * 'init' register data consists of address, mask, value.
	 * Driver can get each data by using LMU_BL_GET_ADDR(),
	 * LMU_BL_GET_MASK(), LMU_BL_GET_VAL().
	 */

#if 0
	for (i = 0; i < num_init; i++) {
		if (!reg)
			break;

		ret = regmap_update_bits(regmap, LMU_BL_GET_ADDR(*reg),
					 LMU_BL_GET_MASK(*reg),
					 LMU_BL_GET_VAL(*reg));
		pr_err("%s ADDR = 0x%x, MASK = 0x%x, VAL = 0x%x\n", __func__,
					 LMU_BL_GET_ADDR(*reg),
					 LMU_BL_GET_MASK(*reg),
					 LMU_BL_GET_VAL(*reg));

		if (ret)
			return ret;

		reg++;
	}
#else
	regmap_write(regmap, 0x10, 0x03);
	regmap_write(regmap, 0x16, 0x00);
	regmap_write(regmap, 0x19, 0x03);
	regmap_write(regmap, 0x1A, 0x0C);
	regmap_write(regmap, 0x1C, 0x0E);
	regmap_write(regmap, 0x22, 0x07);
	regmap_write(regmap, 0x23, 0xFF);
	regmap_write(regmap, 0x24, 0x02);
#endif

	pr_err("[bkl] %s finish\n", __func__);
	return 0;
}

static int ti_lmu_backlight_reload(struct ti_lmu_bl_chip *chip)
{
	struct ti_lmu_bl *each;
	int i, ret;

	ret = ti_lmu_backlight_init(chip);
	if (ret)
		return ret;

	for (i = 0; i < chip->num_backlights; i++) {
		each = chip->lmu_bl + i;
		ret = ti_lmu_backlight_configure(each);
		if (ret)
			return ret;

		backlight_update_status(each->bl_dev);
	}

	return 0;
}

/*static int ti_lmu_backlight_add_device(struct device *dev,
				       struct ti_lmu_bl *lmu_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = lmu_bl->default_brightness;
	props.max_brightness = lmu_bl->chip->cfg->max_brightness;

	bl_dev = backlight_device_register(lmu_bl->name, dev, lmu_bl,
					   &lmu_backlight_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lmu_bl->bl_dev = bl_dev;

	return 0;
}*/

static struct ti_lmu_bl_chip *
ti_lmu_backlight_register(struct device *dev, struct ti_lmu *lmu,
			  const struct ti_lmu_bl_cfg *cfg)
{
	struct ti_lmu_bl_chip *chip;
	struct ti_lmu_bl *each;
	int i, ret;

	pr_err("[bkl] %s enter\n", __func__);

	if (!cfg) {
		dev_err(dev, "Operation is not configured\n");
		return ERR_PTR(-EINVAL);
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return ERR_PTR(-ENOMEM);

	chip->dev = dev;
	chip->lmu = lmu;
	chip->cfg = cfg;

	ret = ti_lmu_backlight_of_create(chip, dev->of_node);
	if (ret)
		return ERR_PTR(ret);

	dump_i2c_reg(chip);
#if 0
	ret = ti_lmu_backlight_init(chip);
	if (ret) {
		dev_err(dev, "Backlight init err: %d\n", ret);
		return ERR_PTR(ret);
	}
#endif

	for (i = 0; i < chip->num_backlights; i++) {
		each = chip->lmu_bl + i;

		ret = ti_lmu_backlight_configure(each);
		if (ret) {
			dev_err(dev, "[bkl] Backlight config err: %d\n", ret);
			return ERR_PTR(ret);
		}

		/*ret = ti_lmu_backlight_add_device(dev, each);
		if (ret) {
			dev_err(dev, "[bkl] Backlight device err: %d\n", ret);
			return ERR_PTR(ret);
		}*/

		//backlight_update_status(each->bl_dev);
	}

	ret = ti_lmu_backlight_init(chip);
	if (ret) {
		dev_err(dev, "Backlight init err: %d\n", ret);
		return ERR_PTR(ret);
	}

	dump_i2c_reg(chip);

	bl_chip = chip;

	pr_err("[bkl] %s finish\n", __func__);

	return chip;
}

static void ti_lmu_backlight_unregister(struct ti_lmu_bl_chip *chip)
{
	struct ti_lmu_bl *each;
	int i;

	/* Turn off the brightness */
	for (i = 0; i < chip->num_backlights; i++) {
		each = chip->lmu_bl + i;
		each->bl_dev->props.brightness = 0;
		backlight_update_status(each->bl_dev);
		backlight_device_unregister(each->bl_dev);
	}
}

static int ti_lmu_backlight_monitor_notifier(struct notifier_block *nb,
					     unsigned long action, void *unused)
{
	struct ti_lmu_bl_chip *chip = container_of(nb, struct ti_lmu_bl_chip,
						   nb);

	if (action == LMU_EVENT_MONITOR_DONE) {
		if (ti_lmu_backlight_reload(chip))
			return NOTIFY_STOP;
	}

	return NOTIFY_OK;
}

static int ti_lmu_backlight_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ti_lmu *lmu = dev_get_drvdata(dev->parent);
	struct ti_lmu_bl_chip *chip;
	int ret;

	pr_err("[bkl] %s enter\n", __func__);

	/* set PM439_GPIO4 output ,HIGH and enable */
	//spmi_register_write(0xC340,0x11);
	//spmi_register_write(0xC346,0x80);
	//writel_relaxed(0x11,0xC340);
	//writel_relaxed(0x80,0xC346);

	chip = ti_lmu_backlight_register(dev, lmu, &lmu_bl_cfg[pdev->id]);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	/*
	 * Notifier callback is required because backlight device needs
	 * reconfiguration after fault detection procedure is done by
	 * ti-lmu-fault-monitor driver.
	 */
	if (chip->cfg->fault_monitor_used) {
		chip->nb.notifier_call = ti_lmu_backlight_monitor_notifier;
		ret = blocking_notifier_chain_register(&chip->lmu->notifier,
						       &chip->nb);
		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, chip);
	pr_err("[bkl] %s finish\n", __func__);
	return 0;
}

static int ti_lmu_backlight_remove(struct platform_device *pdev)
{
	struct ti_lmu_bl_chip *chip = platform_get_drvdata(pdev);

	if (chip->cfg->fault_monitor_used)
		blocking_notifier_chain_unregister(&chip->lmu->notifier,
						   &chip->nb);

	ti_lmu_backlight_unregister(chip);

	return 0;
}

static struct platform_driver ti_lmu_backlight_driver = {
	.probe  = ti_lmu_backlight_probe,
	.remove = ti_lmu_backlight_remove,
	.driver = {
		.name = "ti-lmu-backlight",
	},
};

module_platform_driver(ti_lmu_backlight_driver)

MODULE_DESCRIPTION("TI LMU Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ti-lmu-backlight");
