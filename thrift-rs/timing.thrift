struct timing {
	1: required i64 msgid,
	2: required i64 nanosec,
	3: required string source
}

service Bench {
	i64 bench(1:timing arg)
}
