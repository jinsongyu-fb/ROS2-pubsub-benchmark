struct timing {
	1: i64 msgid,
	2: i64 nanosec,
	3: string source
}

service Bench {
	i64 bench(1:timing arg)
}