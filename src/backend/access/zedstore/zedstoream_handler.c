/*-------------------------------------------------------------------------
 *
 * zedstoream_handler.c
 *	  heap table access method code
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/zedstore/zedstoream_handler.c
 *
 *
 * NOTES
 *	  This file contains the zedstore_ routines which implement
 *	  the POSTGRES zedstore table access method used for all POSTGRES
 *	  relations.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "miscadmin.h"

#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/pg_am_d.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "optimizer/plancat.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "storage/bufmgr.h"

typedef struct ZedStoreDescData
{
	/* scan parameters */
	TableScanDescData rs_scan;  /* */
	int *proj_atts;
	FILE **fds;
	int num_proj_atts;
} ZedStoreDescData;

typedef struct ZedStoreDescData *ZedStoreDesc;
/* ----------------------------------------------------------------
 *				storage AM support routines for zedstoream
 * ----------------------------------------------------------------
 */

static bool
zedstoream_fetch_row_version(Relation relation,
						 ItemPointer tid,
						 Snapshot snapshot,
						 TupleTableSlot *slot,
						 Relation stats_relation)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
	return false;
}

static void
write_datum_to_file(Relation relation, Datum d, int att_num, Form_pg_attribute attr)
{
	char	   *path;
	char *path_col;
	FILE *fd;

	path = relpathperm(relation->rd_node, MAIN_FORKNUM);
	path_col = psprintf("%s.%d", path, att_num+1);
	fd = fopen(path_col, "a");

	if (fd < 0)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m", path)));
	}

	if (!attr->attbyval)
		fwrite(DatumGetPointer(d), 1, attr->attlen, fd);
	else
		fwrite(&d, 1, attr->attlen, fd);
	fflush(fd);
	fclose(fd);
	pfree(path);
	pfree(path_col);
}

/*
 * Insert a heap tuple from a slot, which may contain an OID and speculative
 * insertion token.
 */
static void
zedstoream_insert(Relation relation, TupleTableSlot *slot, CommandId cid,
				   int options, BulkInsertState bistate)
{
	int i;
	Datum *d;
	bool *isnull;
	slot_getallattrs(slot);

	d = slot->tts_values;
	isnull = slot->tts_isnull;

	for(i=0; i < relation->rd_att->natts; i++)
	{
		Form_pg_attribute attr = &relation->rd_att->attrs[i];

		if (attr->attlen < 0)
			elog(LOG, "over ambitious. zedstore is only few weeks old, yet to learn handling variable lengths");

		if (isnull[i])
			elog(ERROR, "you are going too fast. zedstore can't handle NULLs currently.");

		write_datum_to_file(relation, d[i], i, attr);
	}
}

static void
zedstoream_insert_speculative(Relation relation, TupleTableSlot *slot, CommandId cid,
							   int options, BulkInsertState bistate, uint32 specToken)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_complete_speculative(Relation relation, TupleTableSlot *slot, uint32 spekToken,
								 bool succeeded)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}


static HTSU_Result
zedstoream_delete(Relation relation, ItemPointer tid, CommandId cid,
				   Snapshot snapshot, Snapshot crosscheck, bool wait,
				   HeapUpdateFailureData *hufd, bool changingPart)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}


static HTSU_Result
zedstoream_lock_tuple(Relation relation, ItemPointer tid, Snapshot snapshot,
				TupleTableSlot *slot, CommandId cid, LockTupleMode mode,
				LockWaitPolicy wait_policy, uint8 flags,
				HeapUpdateFailureData *hufd)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}


static HTSU_Result
zedstoream_update(Relation relation, ItemPointer otid, TupleTableSlot *slot,
				   CommandId cid, Snapshot snapshot, Snapshot crosscheck,
				   bool wait, HeapUpdateFailureData *hufd,
				   LockTupleMode *lockmode, bool *update_indexes)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_finish_bulk_insert(Relation relation, int options)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static const TupleTableSlotOps *
zedstoream_slot_callbacks(Relation relation)
{
	return &TTSOpsVirtual;
}

static TableScanDesc
zedstoream_beginscan_with_column_projection(Relation relation, Snapshot snapshot,
											int nkeys, ScanKey key,
											ParallelTableScanDesc parallel_scan,
											bool *project_columns,
											bool allow_strat,
											bool allow_sync,
											bool allow_pagemode,
											bool is_bitmapscan,
											bool is_samplescan,
											bool temp_snap)
{
	int i;
	ZedStoreDesc scan;
	char       *path;
	char *path_col;

	/*
	 * allocate and initialize scan descriptor
	 */
	scan = (ZedStoreDesc) palloc(sizeof(ZedStoreDescData));

	scan->rs_scan.rs_rd = relation;
	scan->rs_scan.rs_snapshot = snapshot;
	scan->rs_scan.rs_nkeys = nkeys;
	scan->rs_scan.rs_bitmapscan = is_bitmapscan;
	scan->rs_scan.rs_samplescan = is_samplescan;
	scan->rs_scan.rs_allow_strat = allow_strat;
	scan->rs_scan.rs_allow_sync = allow_sync;
	scan->rs_scan.rs_temp_snap = temp_snap;
	scan->rs_scan.rs_parallel = parallel_scan;

	/*
	 * we can use page-at-a-time mode if it's an MVCC-safe snapshot
	 */
	scan->rs_scan.rs_pageatatime = allow_pagemode && snapshot && IsMVCCSnapshot(snapshot);

	/*
	 * we do this here instead of in initscan() because heap_rescan also calls
	 * initscan() and we don't want to allocate memory again
	 */
	if (nkeys > 0)
		scan->rs_scan.rs_key = (ScanKey) palloc(sizeof(ScanKeyData) * nkeys);
	else
		scan->rs_scan.rs_key = NULL;

	scan->proj_atts = palloc(relation->rd_att->natts * sizeof(int));
	scan->fds = palloc(relation->rd_att->natts * sizeof(FILE*));
	scan->num_proj_atts = 0;

	path = relpathperm(relation->rd_node, MAIN_FORKNUM);
	/*
	 * convert booleans array into an array of the attribute numbers of the
	 * required columns.
	 */
	for (i = 0; i < relation->rd_att->natts; i++)
	{
		/* if project_columns is empty means need all the columns */
		if (project_columns == NULL || project_columns[i])
		{
			scan->proj_atts[scan->num_proj_atts++] = i;
			path_col = psprintf("%s.%d", path, i+1);
			scan->fds[i] = fopen(path_col, "r");
			if (scan->fds[i] < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\": %m", path)));
			pfree(path_col);
		}
	}

	pfree(path);
	return (TableScanDesc) scan;
}

static TableScanDesc
zedstoream_beginscan(Relation relation, Snapshot snapshot,
					 int nkeys, ScanKey key,
					 ParallelTableScanDesc parallel_scan,
					 bool allow_strat,
					 bool allow_sync,
					 bool allow_pagemode,
					 bool is_bitmapscan,
					 bool is_samplescan,
					 bool temp_snap)
{
	return zedstoream_beginscan_with_column_projection(relation, snapshot, nkeys, key, parallel_scan,
													   NULL, allow_strat, allow_sync, allow_pagemode,
													   is_bitmapscan, is_samplescan, temp_snap);
}

static void
zedstoream_setscanlimits(TableScanDesc sscan, BlockNumber startBlk, BlockNumber numBlks)
{
//	heap_setscanlimits((TableScanDesc)((ZedStoreDesc)sscan)->heapscandesc, startBlk, numBlks);
}

static void
zedstoream_endscan(TableScanDesc sscan)
{
	int i;
	ZedStoreDesc scan = (ZedStoreDesc) sscan;
	if (scan->proj_atts)
		pfree(scan->proj_atts);

	for (i = 0; i < sscan->rs_rd->rd_att->natts; i++)
	{
		if (scan->fds[i])
			fclose(scan->fds[i]);
	}

	pfree(scan->fds);
	pfree(scan);
}

static TupleTableSlot *
zedstoream_getnextslot(TableScanDesc sscan, ScanDirection direction, TupleTableSlot *slot)
{
	ZedStoreDesc scan = (ZedStoreDesc) sscan;

	Assert(scan->num_proj_atts <= slot->tts_tupleDescriptor->natts);

	slot->tts_nvalid = 0;
	slot->tts_flags |= TTS_FLAG_EMPTY;

	for (int i = 0; i < scan->num_proj_atts; i++)
	{
		int natt = scan->proj_atts[i];

		fread(&slot->tts_values[natt], 1,
			  slot->tts_tupleDescriptor->attrs[i].attlen, scan->fds[natt]);

		if (ferror(scan->fds[natt]))
			elog(ERROR, "file read failed.");
		if (feof(scan->fds[natt]))
		{
			ExecClearTuple(slot);
			return slot;
		}
		slot->tts_isnull[natt] = false;
	}

	slot->tts_nvalid = slot->tts_tupleDescriptor->natts;
	slot->tts_flags &= ~TTS_FLAG_EMPTY;
	return slot;
}

static bool
zedstoream_tuple_satisfies_snapshot(Relation rel, TupleTableSlot *slot, Snapshot snapshot)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static IndexFetchTableData*
zedstoream_begin_index_fetch(Relation rel)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}


static void
zedstoream_reset_index_fetch(IndexFetchTableData* scan)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_end_index_fetch(IndexFetchTableData* scan)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_fetch_follow(struct IndexFetchTableData *scan,
					ItemPointer tid,
					Snapshot snapshot,
					TupleTableSlot *slot,
					bool *call_again, bool *all_dead)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_scan_bitmap_pagescan(TableScanDesc sscan,
							TBMIterateResult *tbmres)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_scan_bitmap_pagescan_next(TableScanDesc sscan, TupleTableSlot *slot)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_scan_sample_next_block(TableScanDesc sscan, struct SampleScanState *scanstate)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_scan_sample_next_tuple(TableScanDesc sscan, struct SampleScanState *scanstate, TupleTableSlot *slot)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_scan_analyze_next_block(TableScanDesc sscan, BlockNumber blockno, BufferAccessStrategy bstrategy)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static bool
zedstoream_scan_analyze_next_tuple(TableScanDesc sscan, TransactionId OldestXmin, double *liverows, double *deadrows, TupleTableSlot *slot)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_relation_nontransactional_truncate(Relation rel)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_set_new_filenode(Relation rel, char persistence,
						TransactionId *freezeXid, MultiXactId *minmulti)
{
	/*
	 * Initialize to the minimum XID that could put tuples in the table.
	 * We know that no xacts older than RecentXmin are still running, so
	 * that will do.
	 */
	*freezeXid = RecentXmin;

	/*
	 * Similarly, initialize the minimum Multixact to the first value that
	 * could possibly be stored in tuples in the table.  Running
	 * transactions could reuse values from their local cache, so we are
	 * careful to consider all currently running multis.
	 *
	 * XXX this could be refined further, but is it worth the hassle?
	 */
	*minmulti = GetOldestMultiXactId();

	RelationCreateStorage(rel->rd_node, persistence);

	/*
	 * If required, set up an init fork for an unlogged table so that it can
	 * be correctly reinitialized on restart.  An immediate sync is required
	 * even if the page has been logged, because the write did not go through
	 * shared_buffers and therefore a concurrent checkpoint may have moved the
	 * redo pointer past our xlog record.  Recovery may as well remove it
	 * while replaying, for example, XLOG_DBASE_CREATE or XLOG_TBLSPC_CREATE
	 * record. Therefore, logging is necessary even if wal_level=minimal.
	 */
	if (rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED)
	{
		Assert(rel->rd_rel->relkind == RELKIND_RELATION ||
			   rel->rd_rel->relkind == RELKIND_MATVIEW ||
			   rel->rd_rel->relkind == RELKIND_TOASTVALUE);
		RelationOpenSmgr(rel);
		smgrcreate(rel->rd_smgr, INIT_FORKNUM, false);
		log_smgrcreate(&rel->rd_smgr->smgr_rnode.node, INIT_FORKNUM);
		smgrimmedsync(rel->rd_smgr, INIT_FORKNUM);
	}
}

static void
zedstoream_relation_copy_data(Relation rel, RelFileNode newrnode)
{
	ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("function not implemented yet")));
}

static void
zedstoream_estimate_rel_size(Relation rel, int32 *attr_widths,
						 BlockNumber *pages, double *tuples,
						 double *allvisfrac)
{
	BlockNumber curpages;
	BlockNumber relpages;
	double		reltuples;
	BlockNumber relallvisible;
	double		density;

	/* it has storage, ok to call the smgr */
	curpages = RelationGetNumberOfBlocks(rel);

	/* coerce values in pg_class to more desirable types */
	relpages = (BlockNumber) rel->rd_rel->relpages;
	reltuples = (double) rel->rd_rel->reltuples;
	relallvisible = (BlockNumber) rel->rd_rel->relallvisible;

	/*
	 * HACK: if the relation has never yet been vacuumed, use a
	 * minimum size estimate of 10 pages.  The idea here is to avoid
	 * assuming a newly-created table is really small, even if it
	 * currently is, because that may not be true once some data gets
	 * loaded into it.  Once a vacuum or analyze cycle has been done
	 * on it, it's more reasonable to believe the size is somewhat
	 * stable.
	 *
	 * (Note that this is only an issue if the plan gets cached and
	 * used again after the table has been filled.  What we're trying
	 * to avoid is using a nestloop-type plan on a table that has
	 * grown substantially since the plan was made.  Normally,
	 * autovacuum/autoanalyze will occur once enough inserts have
	 * happened and cause cached-plan invalidation; but that doesn't
	 * happen instantaneously, and it won't happen at all for cases
	 * such as temporary tables.)
	 *
	 * We approximate "never vacuumed" by "has relpages = 0", which
	 * means this will also fire on genuinely empty relations.  Not
	 * great, but fortunately that's a seldom-seen case in the real
	 * world, and it shouldn't degrade the quality of the plan too
	 * much anyway to err in this direction.
	 *
	 * If the table has inheritance children, we don't apply this
	 * heuristic. Totally empty parent tables are quite common, so we should
	 * be willing to believe that they are empty.
	 */
	if (curpages < 10 &&
		relpages == 0 &&
		!rel->rd_rel->relhassubclass)
		curpages = 10;

	/* report estimated # pages */
	*pages = curpages;
	/* quick exit if rel is clearly empty */
	if (curpages == 0)
	{
		*tuples = 0;
		*allvisfrac = 0;
		return;
	}

	/* estimate number of tuples from previous tuple density */
	if (relpages > 0)
		density = reltuples / (double) relpages;
	else
	{
		/*
		 * When we have no data because the relation was truncated,
		 * estimate tuple width from attribute datatypes.  We assume
		 * here that the pages are completely full, which is OK for
		 * tables (since they've presumably not been VACUUMed yet) but
		 * is probably an overestimate for indexes.  Fortunately
		 * get_relation_info() can clamp the overestimate to the
		 * parent table's size.
		 *
		 * Note: this code intentionally disregards alignment
		 * considerations, because (a) that would be gilding the lily
		 * considering how crude the estimate is, and (b) it creates
		 * platform dependencies in the default plans which are kind
		 * of a headache for regression testing.
		 */
		int32		tuple_width;

		tuple_width = get_rel_data_width(rel, attr_widths);
		tuple_width += MAXALIGN(SizeofHeapTupleHeader);
		tuple_width += sizeof(ItemIdData);
		/* note: integer division is intentional here */
		density = (BLCKSZ - SizeOfPageHeaderData) / tuple_width;
	}
	*tuples = rint(density * (double) curpages);

	/*
	 * We use relallvisible as-is, rather than scaling it up like we
	 * do for the pages and tuples counts, on the theory that any
	 * pages added since the last VACUUM are most likely not marked
	 * all-visible.  But costsize.c wants it converted to a fraction.
	 */
	if (relallvisible == 0 || curpages <= 0)
		*allvisfrac = 0;
	else if ((double) relallvisible >= curpages)
		*allvisfrac = 1;
	else
		*allvisfrac = (double) relallvisible / curpages;
}

static const TableAmRoutine zedstoream_methods = {
	.type = T_TableAmRoutine,
	.scans_leverage_column_projection = true,

	.slot_callbacks = zedstoream_slot_callbacks,

	.tuple_satisfies_snapshot = zedstoream_tuple_satisfies_snapshot,

	.scan_begin = zedstoream_beginscan,
	.scan_begin_with_column_projection = zedstoream_beginscan_with_column_projection,
	.scansetlimits = zedstoream_setscanlimits,
	.scan_getnextslot = zedstoream_getnextslot,
	.scan_end = zedstoream_endscan,
//	.scan_rescan = heap_rescan,
//	.scan_update_snapshot = heap_update_snapshot,

	.scan_bitmap_pagescan = zedstoream_scan_bitmap_pagescan,
	.scan_bitmap_pagescan_next = zedstoream_scan_bitmap_pagescan_next,

	.scan_sample_next_block = zedstoream_scan_sample_next_block,
	.scan_sample_next_tuple = zedstoream_scan_sample_next_tuple,

	.tuple_fetch_row_version = zedstoream_fetch_row_version,
	.tuple_fetch_follow = zedstoream_fetch_follow,
	.tuple_insert = zedstoream_insert,
	.tuple_insert_speculative = zedstoream_insert_speculative,
	.tuple_complete_speculative = zedstoream_complete_speculative,
	.tuple_delete = zedstoream_delete,
	.tuple_update = zedstoream_update,
	.tuple_lock = zedstoream_lock_tuple,
//	.multi_insert = heap_multi_insert,
	.finish_bulk_insert = zedstoream_finish_bulk_insert,

//	.tuple_get_latest_tid = heap_get_latest_tid,

//	.relation_vacuum = heap_vacuum_rel,
	.scan_analyze_next_block = zedstoream_scan_analyze_next_block,
	.scan_analyze_next_tuple = zedstoream_scan_analyze_next_tuple,
	.relation_nontransactional_truncate = zedstoream_relation_nontransactional_truncate,
//	.relation_copy_for_cluster = heap_copy_for_cluster,
	.relation_set_new_filenode = zedstoream_set_new_filenode,
	.relation_copy_data = zedstoream_relation_copy_data,
//	.relation_sync = heap_sync,
	.relation_estimate_size = zedstoream_estimate_rel_size,

	.begin_index_fetch = zedstoream_begin_index_fetch,
	.reset_index_fetch = zedstoream_reset_index_fetch,
	.end_index_fetch = zedstoream_end_index_fetch,

//	.compute_xid_horizon_for_tuples = heap_compute_xid_horizon_for_tuples,

//	.index_build_range_scan = IndexBuildHeapRangeScan,

//	.index_validate_scan = validate_index_heapscan
};

Datum
zedstore_tableam_handler(PG_FUNCTION_ARGS)
{
	PG_RETURN_POINTER(&zedstoream_methods);
}
