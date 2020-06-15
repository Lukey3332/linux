static int dm_integ_fast_ctr(struct dm_target *ti, unsigned argc, char **argv)
{
	struct dm_integ_fast_c *ic;
	int r;

	ic = kzalloc(sizeof(struct dm_integ_fast_c), GFP_KERNEL);
	if (!ic) {
		ti->error = "Cannot allocate context";
		return -ENOMEM;
	}
	ti->private = ic;
	ti->per_io_data_size = sizeof(struct dm_integ_fast_io);
	ic->ti = ti;

	r = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &ic->dev);
	if (r) {
		ti->error = "Device lookup failed";
		goto bad;
	}

	r = bioset_init(&ic->bio_set, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);
	if (r) {
		ti->error = "bioset_init failed";
		goto bad;
	}

	return 0;

bad:
	dm_integ_fast_dtr(ti);
	return r;
}

static void dm_integ_fast_dtr(struct dm_target *ti)
{
	struct dm_integ_fast_c *ic = ti->private;
	bioset_exit(&ic->bio_set);
	kfree(ic);
}

static void integ_fast_end_io(struct bio *bio)
{
	struct dm_integ_fast_io *dio = bio->bi_private;
	struct bio *orig_bio = dm_bio_from_per_bio_data(dio, sizeof(struct dm_integ_fast_io));
	if (!orig_bio->bi_status) {
		orig_bio->bi_status = bio->bi_status;
	}
	bio_put(bio);
	bio_endio(orig_bio);
}

static int dm_integ_fast_map(struct dm_target *ti, struct bio *orig_bio)
{
	struct dm_integ_fast_c *ic = ti->private;
	struct dm_integ_fast_io *dio = dm_per_bio_data(orig_bio, sizeof(struct dm_integ_fast_io));
	struct bio *bio;

	bio = bio_clone_fast(orig_bio, GFP_NOIO, &ic->bio_set);
	bio_set_dev(bio, ic->dev->bdev);
	bio->bi_end_io = integ_fast_end_io;
	bio->bi_private = dio;
	dio->bio = bio;
	dio->ic = ic;
	
	// TODO: Make sure that the bio isn't going to be split
}

static struct target_type integrity_target = {
	.name			= "integ-fast",
	.version		= {1, 0, 0},
	.module			= THIS_MODULE,
	.features		= DM_TARGET_SINGLETON,
	.ctr			= dm_integ_fast_ctr,
	.dtr			= dm_integ_fast_dtr,
	.map			= dm_integ_fast_map,
	.postsuspend		= dm_integ_fast_postsuspend,
	.resume			= dm_integ_fast_resume,
	.status			= dm_integ_fast_status,
	.iterate_devices	= dm_integ_fast_iterate_devices,
	.io_hints		= dm_integ_fast_io_hints,
};

static int __init dm_integ_fast_init(void)
{
	int r;

	r = dm_register_target(&integrity_target);

	if (r < 0)
		DMERR("register failed %d", r);

	return r;
}

static void __exit dm_integ_fast_exit(void)
{
	dm_unregister_target(&integrity_target);
}

module_init(dm_integ_fast_init);
module_exit(dm_integ_fast_exit);

MODULE_AUTHOR("Lukas Straub");
MODULE_DESCRIPTION(DM_NAME " target for fast integrity tags extension");
MODULE_LICENSE("GPL");
