#define			LONG_MAX		2147483647L	/*0x7FFFFFFF */
#define			LONG_MIN		(-2147483647L-1L) /*-0x80000000*/

int strtol(const char *nptr, char **endptr, int base)
{
	const char *p = nptr;
	unsigned long ret;
	int ch;
	unsigned long Overflow;
	int sign = 0, flag, LimitRemainder;
	/*delete space*/
	do {
		ch = *p++;
	} while (isspace(ch));
	if (ch == '-') {
		sign = 1;
		ch = *p++;
	}
	else if (ch == '+')
		ch = *p++;
	if ((base == 0 || base == 16) && ch == '0' && (*p == 'x' || *p == 'X')) {
		ch = p[1];
		p += 2;
		base = 16;
	}
	if (base == 0)
		base = ch == '0' ? 8 : 10;
	Overflow = sign ? -(unsigned long)LONG_MIN : LONG_MAX;
	LimitRemainder = Overflow % (unsigned long)base;
	Overflow /= (unsigned long)base;
	for (ret = 0, flag = 0;; ch = *p++) {
		/*get target value*/
		if (isdigit(ch))
			ch -= '0';
		else if (isalpha(ch))
			ch -= isupper(ch) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (ch >= base)
			break;
		/*overflow*/
		if (flag < 0 || ret > Overflow
			|| (ret == Overflow && ch > LimitRemainder))
			flag = -1;
		else {
			flag = 1;
			ret *= base;
			ret += ch;
		}
	}
	if (flag < 0)
		ret = sign ? LONG_MIN : LONG_MAX;
	else if (sign)
		ret = -ret;
	if (endptr != 0)
		*endptr = (char *)(flag ? (p - 1) : nptr);
	return ret;
}
