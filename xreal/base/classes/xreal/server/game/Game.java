package xreal.server.game;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import javax.vecmath.Vector3f;

import com.bulletphysics.collision.broadphase.AxisSweep3;
import com.bulletphysics.collision.broadphase.BroadphaseInterface;
import com.bulletphysics.collision.dispatch.CollisionDispatcher;
import com.bulletphysics.collision.dispatch.CollisionObject;
import com.bulletphysics.collision.dispatch.DefaultCollisionConfiguration;
import com.bulletphysics.collision.shapes.CollisionShape;
import com.bulletphysics.collision.shapes.StaticPlaneShape;
import com.bulletphysics.dynamics.DiscreteDynamicsWorld;
import com.bulletphysics.dynamics.DynamicsWorld;
import com.bulletphysics.dynamics.RigidBody;
import com.bulletphysics.dynamics.RigidBodyConstructionInfo;
import com.bulletphysics.dynamics.constraintsolver.ConstraintSolver;
import com.bulletphysics.dynamics.constraintsolver.SequentialImpulseConstraintSolver;
import com.bulletphysics.linearmath.DefaultMotionState;
import com.bulletphysics.linearmath.Transform;

import xreal.CVars;
import xreal.CollisionBspReader;
import xreal.Engine;
import xreal.common.Config;
import xreal.common.ConfigStrings;
import xreal.common.GameType;
import xreal.server.Server;

public class Game implements GameListener {
	
	static private int levelTime = 0;
	static private int deltaTime = 0;
	
	static private Set<GameEntity> entities;
	static private Set<Player> players;
	
	// keep the collision shapes, for deletion/cleanup
	static private List<CollisionShape> collisionShapes;
	static private BroadphaseInterface overlappingPairCache;
	static private CollisionDispatcher dispatcher;
	static private ConstraintSolver solver;
	static private DefaultCollisionConfiguration collisionConfiguration;
	
	// 
	private static DynamicsWorld dynamicsWorld = null;
	
	// maximum number of objects (and allow user to shoot additional boxes)
	private static final int MAX_PROXIES = 1024;

	private Game() {
		
	}

	@Override
	public String clientConnect(Player client, boolean firstTime, boolean isBot) {
		Engine.print("xreal.server.game.Game.clientConnect(clientNum = " + client.getEntityState_number() + ", firstTime = " + firstTime + ", isBot = " + isBot + ")\n");
		
		//return "Game.clientConnect() is not implemented yet.";	// deny message
		return null;
	}

	@Override
	public boolean consoleCommand() {
		//Engine.print("xreal.server.game.Game.consoleCommand()\n");
		
		String cmd = Engine.getConsoleArgv(0);
		
		if(CVars.g_dedicated.getBoolean())
		{
			if(cmd.equals("say"))
			{
				Server.broadcastClientCommand("print \"server: " + Engine.concatConsoleArgs(1)  + "\n\"");
				return true;
			}
			
			// everything else will also be printed as a say command
			Server.broadcastClientCommand("print \"server: " + Engine.concatConsoleArgs(0) + "\n\"");
			return true;
		}
		
		return false;
	}

	@Override
	public void initGame(int _levelTime, int randomSeed, boolean restart) {
		
		Engine.print("xreal.server.game.Game.initGame(levelTime = "+ levelTime + ", randomSeed = " + randomSeed + ", restart = " + restart + ")\n");
		
		//crashTest();
		
		Engine.print("------- Game Initialization -------\n");
		
		entities = new LinkedHashSet<GameEntity>();
		players = new LinkedHashSet<Player>();
		
		//Engine.println("gamename: "Config.GAME_VERSION);
		//Engine.print("gamedate: %s\n", __DATE__);

		//Engine.sendConsoleCommand(Engine.EXEC_APPEND, "echo cool!");
		
		levelTime = _levelTime;
		deltaTime = 0;
		
		// make some data visible to connecting client
		Server.setConfigString(ConfigStrings.GAME_VERSION, Config.GAME_VERSION);
		Server.setConfigString(ConfigStrings.LEVEL_START_TIME, Integer.toString(levelTime));
		Server.setConfigString(ConfigStrings.MOTD, CVars.g_motd.getString());
		
		Engine.println("Game Version: " + Server.getConfigString(ConfigStrings.GAME_VERSION));
		Engine.println("Game Type: " + GameType.values()[CVars.g_gametype.getInteger()]);
		
		initPhysics();
		
//		for(int i = 0; i < 30; i++)
//		{
//			GameEntity e1 = new GameEntity();
//			
//			/*
//			Engine.println(e1.toString());
//			try {
//				e1.finalize();
//			} catch (Throwable e) {
//				// TODO Auto-generated catch block
//				e.printStackTrace();
//			}
//			*/
//		}
		
		
		Engine.print("-----------------------------------\n");
	}

	@Override
	public void runAIFrame(int time) {
		//Engine.print("xreal.server.game.Game.runAIFrame(time = " + time + ")\n");

	}

	@Override
	public void runFrame(int time) {
		//Engine.print("xreal.server.game.Game.runFrame(time = " + time + ")\n");
		
		deltaTime = time - levelTime;
		levelTime = time;
		
		runPhysics();
		
		// go through all allocated objects
		int start = Engine.getTimeInMilliseconds();
		
		for(GameEntity ent : entities)
		{
			/*
			if(!ent.inuse)
			{
				continue;
			}
			*/

			/*
			// clear events that are too old
			if(level.time - ent->eventTime > EVENT_VALID_MSEC)
			{
				if(ent->s.event)
				{
					ent->s.event = 0;	// &= EV_EVENT_BITS;
					if(ent->client)
					{
						ent->client->ps.externalEvent = 0;
						// predicted events should never be set to zero
						//ent->client->ps.events[0] = 0;
						//ent->client->ps.events[1] = 0;
					}
				}
				if(ent->freeAfterEvent)
				{
					// tempEntities or dropped items completely go away after their event
					G_FreeEntity(ent);
					continue;
				}
				else if(ent->unlinkAfterEvent)
				{
					// items that will respawn will hide themselves after their pickup event
					ent->unlinkAfterEvent = qfalse;
					trap_UnlinkEntity(ent);
				}
			}

			// temporary entities don't think
			if(ent->freeAfterEvent)
			{
				continue;
			}

			if(!ent->r.linked && ent->neverFree)
			{
				continue;
			}
			*/

			/*
			if(ent->s.eType == ET_PROJECTILE || ent->s.eType == ET_PROJECTILE2)
			{
				G_RunMissile(ent);
				continue;
			}

			if(ent->s.eType == ET_ITEM || ent->physicsObject)
			{
				G_RunItem(ent);
				continue;
			}

			if(ent->s.eType == ET_MOVER)
			{
				G_RunMover(ent);
				continue;
			}

			if(i < MAX_CLIENTS)
			{
				G_RunClient(ent);
				continue;
			}
			*/

			ent.runThink();
		}
		int end = Engine.getTimeInMilliseconds();

		/*
		start = Engine.getTimeInMilliseconds();

		// perform final fixups on the players
		ent = &g_entities[0];
		for(i = 0; i < level.maxclients; i++, ent++)
		{
			if(ent->inuse)
			{
				ClientEndFrame(ent);
			}
		}
		end = Engine.getTimeInMilliseconds();
		*/

		// see if it is time to do a tournament restart
		//checkTournament();

		// see if it is time to end the level
		//checkExitRules();

		// update to team status?
		//checkTeamStatus();

		// cancel vote if timed out
		//checkVote();

		// check team votes
		//checkTeamVote(TEAM_RED);
		//checkTeamVote(TEAM_BLUE);
		
		//CVars.g_gametype.set("99");
		//CVars.g_gametype = null;
		//Engine.print(CVars.g_gametype.toString() + "\n");
		
		//System.gc();
		
		//Engine.print("xreal.server.game.Game.runFrame(time2 = " + Engine.getTimeInMilliseconds() + ")\n");
	}

	@Override
	public void shutdownGame(boolean restart) {
		Engine.print("xreal.server.game.Game.shutdownGame(restart = " + restart + ")\n");
	}
	
	static public int getLevelTime() {
		return levelTime;
	}
	
	private void initPhysics() {

		Engine.println("Game.initPhysics()");
		
		collisionShapes = new ArrayList<CollisionShape>();
		
		// collision configuration contains default setup for memory, collision
		// setup
		collisionConfiguration = new DefaultCollisionConfiguration();

		// use the default collision dispatcher. For parallel processing you can
		// use a diffent dispatcher (see Extras/BulletMultiThreaded)
		dispatcher = new CollisionDispatcher(collisionConfiguration);

		// the maximum size of the collision world. Make sure objects stay
		// within these boundaries
		// TODO: AxisSweep3
		// Don't make the world AABB size too large, it will harm simulation
		// quality and performance
		Vector3f worldAabbMin = new Vector3f(-10000, -10000, -10000);
		Vector3f worldAabbMax = new Vector3f(10000, 10000, 10000);
		overlappingPairCache = new AxisSweep3(worldAabbMin, worldAabbMax);//, MAX_PROXIES);
		// overlappingPairCache = new SimpleBroadphase(MAX_PROXIES);

		// the default constraint solver. For parallel processing you can use a
		// different solver (see Extras/BulletMultiThreaded)
		SequentialImpulseConstraintSolver sol = new SequentialImpulseConstraintSolver();
		solver = sol;

		// TODO: needed for SimpleDynamicsWorld
		// sol.setSolverMode(sol.getSolverMode() &
		// ~SolverMode.SOLVER_CACHE_FRIENDLY.getMask());

		dynamicsWorld = new DiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
		// dynamicsWorld = new SimpleDynamicsWorld(dispatcher,
		// overlappingPairCache, solver, collisionConfiguration);

		dynamicsWorld.setGravity(new Vector3f(CVars.g_gravityX.getValue(), CVars.g_gravityY.getValue(), CVars.g_gravityZ.getValue()));

		// create a few basic rigid bodies
		// CollisionShape groundShape = new BoxShape(new Vector3f(50f, 50f,
		// 50f));
		/*
		CollisionShape groundShape = new StaticPlaneShape(new Vector3f(0, 0, 1), 0);

		collisionShapes.add(groundShape);

		Transform groundTransform = new Transform();
		groundTransform.setIdentity();
		groundTransform.origin.set(0, -56, 0);
		
		{
			float mass = 0f;

			// rigidbody is dynamic if and only if mass is non zero, otherwise static
			boolean isDynamic = (mass != 0f);

			Vector3f localInertia = new Vector3f(0, 0, 0);
			if (isDynamic) {
				groundShape.calculateLocalInertia(mass, localInertia);
			}

			// using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
			DefaultMotionState myMotionState = new DefaultMotionState(groundTransform);
			RigidBodyConstructionInfo rbInfo = new RigidBodyConstructionInfo(mass, myMotionState, groundShape, localInertia);
			RigidBody body = new RigidBody(rbInfo);
			//body.

			// add the body to the dynamics world
			dynamicsWorld.addRigidBody(body);
		}
		*/
		
		initCollisionWorld();
		
		//System.gc();
	}
	
	private void initCollisionWorld() {
		Engine.println("Game.initCollisionWorld()");
		
		CollisionBspReader bsp = new CollisionBspReader("maps/" + CVars.g_mapname.getString() + ".bsp");
		
		bsp.addWorldBrushesToSimulation(collisionShapes, dynamicsWorld);
	}
	
	private void runPhysics() {
		dynamicsWorld.stepSimulation(deltaTime * 0.001f, 10);
		
		//Engine.println("Game.runPhysics(): collision objects = " + dynamicsWorld.getNumCollisionObjects());
		
		dynamicsWorld.setGravity(new Vector3f(CVars.g_gravityX.getValue(), CVars.g_gravityY.getValue(), CVars.g_gravityZ.getValue()));

		// print positions of all objects
		for (int j = dynamicsWorld.getNumCollisionObjects() - 1; j >= 0; j--) {
			
			CollisionObject obj = dynamicsWorld.getCollisionObjectArray().get(j);
			RigidBody body = RigidBody.upcast(obj);
			
			if (body != null && body.getMotionState() != null) {
				//Transform trans = new Transform();
				//body.getMotionState().getWorldTransform(trans);
				
				GameEntity ent = (GameEntity) body.getUserPointer();
				if (ent != null) {
					ent.updateEntityStateByPhysics();
				}
			}
		}
	}
	
	private void crashTest() {
		
		//try
		{
			GameEntity ent = null;
		
			//ent.updateEntityStateByPhysics();
		
			Vector3f v1 = null;
			Vector3f v2 = new Vector3f(v1);
		}
		/*
		catch(Exception e)
		{
			Engine.println("exception in Game.crashTest(): " + e.getMessage());
		}
		*/
	}
	
	public static Set<GameEntity> getEntities() {
		return entities;
	}
	
	public static Set<Player> getPlayers() {
		return players;
	}
	
	static public List<CollisionShape> getCollisionShapes() {
		return collisionShapes;
	}

	static public DynamicsWorld getDynamicsWorld() {
		return dynamicsWorld;
	}
}