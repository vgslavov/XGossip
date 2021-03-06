#include <chord.h>
#include "merkle_syncer.h"
#include "qhash.h"
#include "async.h"
#include "bigint.h"
#include <id_utils.h>
#include <comm.h>

#include <modlogger.h>
#define warning modlogger ("merkle", modlogger::WARNING)
#define info  modlogger ("merkle", modlogger::INFO)
#define trace modlogger ("merkle", modlogger::TRACE)


// ---------------------------------------------------------------------------

class merkle_getkeyrange {
private:
  uint vnode;
  dhash_ctype ctype;
  bigint rngmin;
  bigint rngmax;
  bigint current;
  missingfnc_t missing;
  rpcfnc_t rpcfnc;
  vec<chordID> lkeys;


  void go ();
  void getkeys_cb (ref<getkeys_arg> arg, ref<getkeys_res> res, clnt_stat err);
  void doRPC (int procno, ptr<void> in, void *out, aclnt_cb cb);

public:
  ~merkle_getkeyrange () {}
  merkle_getkeyrange (uint vnode, dhash_ctype ctype, 
		      bigint rngmin, bigint rngmax, 
		      vec<chordID> plkeys,
		      missingfnc_t missing, rpcfnc_t rpcfnc)
    : vnode (vnode), ctype (ctype), rngmin (rngmin), 
      rngmax (rngmax), current (rngmin), 
      missing (missing), rpcfnc (rpcfnc), lkeys (plkeys)
    { go (); }
};


// ---------------------------------------------------------------------------
// util junk


// Check whether [l1, r1] overlaps [l2, r2] on the circle.
static bool
overlap (const bigint &l1, const bigint &r1, const bigint &l2, const bigint &r2)
{
  // There might be a more efficient way to do this..
  return (betweenbothincl (l1, r1, l2) || betweenbothincl (l1, r1, r2)
	  || betweenbothincl (l2, r2, l1) || betweenbothincl (l2, r2, r1));
}


// ---------------------------------------------------------------------------
// merkle_syncer


merkle_syncer::merkle_syncer (uint vnode, dhash_ctype ctype, 
			      ptr<merkle_tree> ltree, 
			      rpcfnc_t rpcfnc, missingfnc_t missingfnc)
  : vnode (vnode), ctype (ctype), ltree (ltree), rpcfnc (rpcfnc), 
    missingfnc (missingfnc), completecb (cbi_null)
{
  deleted = New refcounted<bool> (false);
  fatal_err = NULL;
  sync_done = false;
}

merkle_syncer::~merkle_syncer ()
{
  *deleted = true;
}

void
merkle_syncer::sync (bigint rngmin, bigint rngmax, cbi cb)
{
  local_rngmin = rngmin;
  local_rngmax = rngmax;
  completecb = cb;

  // start at the root of the merkle tree
  sendnode (0, 0);
}


void
merkle_syncer::dump ()
{    
  //warn << "THIS: " << (u_int)this << "\n";
  warn << "  st.size () " << st.size () << "\n"; 
}


void
merkle_syncer::doRPC (int procno, ptr<void> in, void *out, aclnt_cb cb)
{
  // Must resort to bundling all values into one argument since
  // async/callback.h is configured with too few parameters.
  struct RPC_delay_args args (merklesync_program_1, procno, in, out, cb, 
			      NULL);
  (*rpcfnc) (&args);
}

void
merkle_syncer::setdone ()
{
  if (completecb != cbi_null) 
    // Return 0 if everything was ok, 1 otherwise.
    completecb (fatal_err != NULL);
  sync_done = true;
}


void
merkle_syncer::error (str err)
{
  //warn << (u_int)this << ": SYNCER ERROR: " << err << "\n";
  fatal_err = err;
  setdone ();
}

str
merkle_syncer::getsummary ()
{
  assert (sync_done);
  strbuf sb;

  sb << "[" << local_rngmin << "," << local_rngmax << "] ";

  if (fatal_err)
    sb << fatal_err;

  return sb;
}

void
merkle_syncer::next (void)
{
  trace << "local range [" <<  local_rngmin << "," << local_rngmax << "]\n";
  assert (!sync_done);
  assert (!fatal_err);

  // st is queue of pending index nodes
  while (st.size ()) {
    pair<merkle_rpc_node, int> &p = st.back ();
    merkle_rpc_node *rnode = &p.first;
    assert (!rnode->isleaf);
    
    merkle_node *lnode = ltree->lookup_exact (rnode->depth, rnode->prefix);

    if (!lnode) {
      fatal << "lookup_exact didn't match for " << rnode->prefix << " at depth " << rnode->depth << "\n";
    }
    

    trace << "starting from slot " << p.second << "\n";

    while (p.second < 64) {
      u_int i = p.second;
      p.second += 1;
      trace << "CHECKING: " << i;

      bigint remote = tobigint (rnode->child_hash[i]);
      bigint local = tobigint (lnode->child_hash(i));

      u_int depth = rnode->depth + 1;

      //prefix is the high bits of the first key 
      // the node is responsible for.
      merkle_hash prefix = rnode->prefix;
      prefix.write_slot (rnode->depth, i);
      bigint slot_rngmin = tobigint (prefix);
      bigint slot_width = bigint (1) << (160 - 6*depth);
      bigint slot_rngmax = slot_rngmin + slot_width - 1;

      bool overlaps = overlap (local_rngmin, local_rngmax, slot_rngmin, slot_rngmax);

      strbuf tr;
      if (remote != local) {
	tr << " differ. local " << local << " != remote " << remote;

	if (overlaps) {
	  tr << " .. sending\n";
	  sendnode (depth, prefix);
	  trace << tr;
	  ltree->lookup_release(lnode);
	  return;
	} else {
	  tr << " .. not sending\n";
	}
      } else {
	tr << " same. local " << local << " == remote " << remote << "\n";
      }
      trace << tr;
    }

    ltree->lookup_release(lnode);
    assert (p.second == 64);
    st.pop_back ();
  }

  trace << "DONE .. in NEXT\n";
  setdone ();
  trace << "OK!\n";
}


void
merkle_syncer::sendnode (u_int depth, const merkle_hash &prefix)
{

  ref<sendnode_arg> arg = New refcounted<sendnode_arg> ();
  ref<sendnode_res> res = New refcounted<sendnode_res> ();

  u_int lnode_depth;
  merkle_node *lnode = ltree->lookup (&lnode_depth, depth, prefix);
  // OK to assert this: since depth-1 is an index node, we know that
  //                    it created all of its depth children when
  //                    it split. --FED
  assert (lnode);
  assert (lnode_depth == depth);

  format_rpcnode (ltree, depth, prefix, lnode, &arg->node);
  arg->vnode = vnode;
  arg->ctype = ctype;
  arg->rngmin = local_rngmin;
  arg->rngmax = local_rngmax;
  ltree->lookup_release(lnode);
  doRPC (MERKLESYNC_SENDNODE, arg, res,
	 wrap (this, &merkle_syncer::sendnode_cb, deleted, arg, res));
}


void
merkle_syncer::sendnode_cb (ptr<bool> deleted,
			    ref<sendnode_arg> arg, ref<sendnode_res> res, 
			    clnt_stat err)
{
  if (*deleted)
    return;
  if (err) {
    error (strbuf () << "SENDNODE: rpc error " << err);
    return;
  } else if (res->status != MERKLE_OK) {
    error (strbuf () << "SENDNODE: protocol error " << err2str (res->status));
    return;
  }

  merkle_rpc_node *rnode = &res->resok->node;

  merkle_node *lnode = ltree->lookup_exact (rnode->depth, rnode->prefix);
  if (!lnode) {
    fatal << "lookup failed: " << rnode->prefix << " at " << rnode->depth << "\n";
  }
  
  compare_nodes (ltree, local_rngmin, local_rngmax, lnode, rnode, 
		 vnode, ctype, missingfnc, rpcfnc);

  if (!lnode->isleaf () && !rnode->isleaf) {
    trace << "I vs I\n";
    st.push_back (pair<merkle_rpc_node, int> (*rnode, 0));
  }

  ltree->lookup_release(lnode);

  next ();
}



// ---------------------------------------------------------------------------
// merkle_getkeyrange

void
merkle_getkeyrange::go ()
{
  if (!betweenbothincl (rngmin, rngmax, current)) {
    trace << "merkle_getkeyrange::go () ==> DONE\n";
    delete this;
    return;
  }

  ref<getkeys_arg> arg = New refcounted<getkeys_arg> ();
  arg->ctype = ctype;
  arg->vnode = vnode;
  arg->rngmin = current;
  arg->rngmax = rngmax;
  ref<getkeys_res> res = New refcounted<getkeys_res> ();
  doRPC (MERKLESYNC_GETKEYS, arg, res,
	 wrap (this, &merkle_getkeyrange::getkeys_cb, arg, res));
}



void
merkle_getkeyrange::getkeys_cb (ref<getkeys_arg> arg, ref<getkeys_res> res, 
				clnt_stat err)

{
  if (err) {
    warn << "GETKEYS: rpc error " << err << "\n";
    delete this;
    return;
  } else if (res->status != MERKLE_OK) {
    warn << "GETKEYS: protocol error " << err2str (res->status) << "\n";
    delete this;
    return;
  }

  // Assuming keys are sent back in increasing clockwise order
  vec<chordID> rkeys;
  for (u_int i = 0; i < res->resok->keys.size (); i++) 
    rkeys.push_back (res->resok->keys[i]);

  chordID sentmax = rngmax;
  if (res->resok->keys.size () > 0)
    sentmax = res->resok->keys.back ();
  compare_keylists (lkeys, rkeys, current, sentmax, missing);

  current = incID (sentmax);
  if (!res->resok->morekeys)
    current = incID (rngmax);  // set done
  
  go ();
}


void
merkle_getkeyrange::doRPC (int procno, ptr<void> in, void *out, aclnt_cb cb)
{
  // Must resort to bundling all values into one argument since
  // async/callback.h is configured with too few parameters.
  struct RPC_delay_args args (merklesync_program_1, procno, in, out, cb,
			      NULL);
  (*rpcfnc) (&args);
}


// ---------------------------------------------------------------------------

void
compare_keylists (vec<chordID> lkeys,
		  vec<chordID> vrkeys,
		  chordID rngmin, chordID rngmax,
		  missingfnc_t missingfnc)
{
  // populate a hash table with the remote keys
  qhash<chordID, int, hashID> rkeys;
  for (u_int i = 0; i < vrkeys.size (); i++) {
    if (betweenbothincl (rngmin, rngmax, vrkeys[i])) {
      // trace << "remote key: " << vrkeys[i] << "\n";
      rkeys.insert (vrkeys[i], 1);
    }
  }
    
  // do I have something he doesn't have?
  for (unsigned int i = 0; i < lkeys.size (); i++) {
    if (!rkeys[lkeys[i]] && 
	betweenbothincl (rngmin, rngmax, lkeys[i])) {
      trace << "remote missing [" << rngmin << ", " 
	    << rngmax << "] key=" << lkeys[i] << "\n";
      (*missingfnc) (lkeys[i], false);
    } else {
      if (rkeys[lkeys[i]]) trace << "remote has " << lkeys[i] << "\n";
      else trace << "out of range: " << lkeys[i] << "\n";
      rkeys.remove (lkeys[i]);
    }
  }

  //anything left: he has and I don't
  qhash_slot<chordID, int> *slot = rkeys.first ();
  while (slot) {
    trace << "local missing [" << rngmin << ", " 
	  << rngmax << "] key=" << slot->key << "\n";
    (*missingfnc) (slot->key, true);
    slot = rkeys.next (slot);
  }
   
}

void
compare_nodes (merkle_tree *ltree, bigint rngmin, bigint rngmax, 
	       merkle_node *lnode, merkle_rpc_node *rnode,
	       uint vnode, dhash_ctype ctype,
	       missingfnc_t missingfnc, rpcfnc_t rpcfnc)
{
  trace << (lnode->isleaf ()  ? "L" : "I")
       << " vs "
       << (rnode->isleaf ? "L" : "I")
       << "\n";

  vec<chordID> lkeys = ltree->database_get_IDs (rnode->depth, 
						rnode->prefix);
  if (rnode->isleaf) {
    
    vec<chordID> rkeys;
    for (u_int i = 0; i < rnode->child_hash.size (); i++) 
	rkeys.push_back (tobigint(rnode->child_hash[i]));

    compare_keylists (lkeys, rkeys, rngmin, rngmax, missingfnc);    
	
  } else if (lnode->isleaf () && !rnode->isleaf) {
    bigint tmpmin = tobigint (rnode->prefix);
    bigint node_width = bigint (1) << (160 - rnode->depth);
    bigint tmpmax = tmpmin + node_width - 1;

    // further constrain to be within the host's range of interest
    if (between (tmpmin, tmpmax, rngmin))
      tmpmin = rngmin;
    if (between (tmpmin, tmpmax, rngmax))
      tmpmax = rngmax;

    vNew merkle_getkeyrange (vnode, ctype, tmpmin, tmpmax, lkeys, 
			     missingfnc, rpcfnc);
  }
}

// ---------------------------------------------------------------------------

void
format_rpcnode (merkle_tree *ltree, u_int depth, const merkle_hash &prefix,
		merkle_node *node, merkle_rpc_node *rpcnode)
{
  rpcnode->depth = depth;
  rpcnode->prefix = prefix;
  rpcnode->count = node->count;
  rpcnode->hash = node->hash;
  rpcnode->isleaf = node->isleaf ();
  
  if (!node->isleaf ()) {
    rpcnode->child_hash.setsize (64);
    for (int i = 0; i < 64; i++)
      rpcnode->child_hash[i] = node->child_hash(i);
  } else {
    vec<merkle_hash> keys = ltree->database_get_keys (depth, prefix);

    if (keys.size () != rpcnode->count) {
      warn << "\n\n\n----------------------------------------------------------\n";
      warn << "BUG BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG BUG\n";
      warn << "Send this output to chord@pdos.lcs.mit.edu\n";
      warn << "BUG: " << depth << " " << prefix << "\n";
      warn << "BUG: " << keys.size () << " != " << rpcnode->count << "\n";
      ltree->check_invariants ();
      warn << "BUG BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG  BUG BUG\n";
      panic << "----------------------------------------------------------\n\n\n";
    }

    rpcnode->child_hash.setsize (keys.size ());
    for (u_int i = 0; i < keys.size (); i++) {
      rpcnode->child_hash[i] = keys[i];
    }
  }
}
