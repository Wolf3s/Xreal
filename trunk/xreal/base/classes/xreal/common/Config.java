package xreal.common;

public class Config {
	/**
	 * Always increase this if you change the ET_, EV_, GT_ or WP_ types.
	 */
	public static final String GAME_VERSION = "XreaL-rev4";

	public static final String DEFAULT_MODEL = "shina";
	public static final String DEFAULT_HEADMODEL = "shina";

	
	// Tr3B: changed to HL 2 / Quake 4 properties
	public static final int STEPSIZE = 18;

	public static final int DEFAULT_VIEWHEIGHT = 44; // 68 // Tr3B: was 26

	public static final int CROUCH_VIEWHEIGHT = 16; // 32 // Tr3B: was 12
	public static final int CROUCH_HEIGHT = 20; // 38 // Tr3B: was 16
	public static final int DEAD_VIEWHEIGHT = -16; // Tr3B: was -16
}