models/weapons2/gauntlet/ldagauntlet
{
	{
		blend diffuseMap
		map models/weapons2/gauntlet/ldagauntlet_d.tga
		rgbGen lightingDiffuse
	}
	{
		blend bumpMap
		map heightMap(models/weapons2/gauntlet/ldagauntlet_h.tga, 2)
	}
	{
		blend specularMap
		map models/weapons2/gauntlet/ldagauntlet_s.tga
	}
}

models/weapons2/gauntlet/ldagauntlet_b
{
	{
		blend diffuseMap
		map models/weapons2/gauntlet/ldagauntlet_b_d.tga
		rgbGen lightingDiffuse
		//centerScale 1.2, 1.2
	}
	{
		blend bumpMap
		map heightMap(models/weapons2/gauntlet/ldagauntlet_b_h.tga, 1)
		//map models/weapons2/gauntlet/ldagauntlet_b_n.tga
	}
	{
		blend specularMap
		map models/weapons2/gauntlet/ldagauntlet_b_s.tga
	}
}

models/weapons2/gauntlet/sparky1
{
	twoSided
	
	{
		animMap 13 models/weapons2/gauntlet/sparky1.tga models/weapons2/gauntlet/sparky2.tga models/weapons2/gauntlet/sparky3.tga models/weapons2/gauntlet/sparky4.tga models/weapons2/gauntlet/sparky5.tga models/weapons2/gauntlet/sparky6.tga models/weapons2/gauntlet/sparky7.tga 
		blend add
		rgbGen wave sin 0.3 1.1 0 17 
	}
	{
		animMap 22 models/weapons2/gauntlet/sparky1.tga models/weapons2/gauntlet/sparky2.tga models/weapons2/gauntlet/sparky3.tga models/weapons2/gauntlet/sparky4.tga models/weapons2/gauntlet/sparky5.tga models/weapons2/gauntlet/sparky6.tga models/weapons2/gauntlet/sparky7.tga 
		blend add
		rgbGen wave sin 0.8 0.1 0 5 
	}
}

models/weapons2/gauntlet/ldagauntlet_ifx
{
	twoSided
	
	{
		map models/weapons2/gauntlet/ldagauntlet_ifx.tga
		blend add
		rgbGen wave sin 0.1 0.6 0 9 
		tcMod rotate 512
		tcMod transform 0.9 0 0 0.9 1.05 1.05
	}
}

models/weapons2/gauntlet/ldagauntlet_afx1
{
	twoSided
	
	{
		map models/weapons2/gauntlet/ldagauntlet_afx1.tga
		blend add
		tcMod scroll 5 0
		tcMod turb 0.01 0.09 0 9
	}
	{
		map models/weapons2/gauntlet/ldagauntlet_afx2.tga
		blend add
		rgbGen wave sin 0.1 0.9 88 9 
		tcMod scroll -11 0
	}
	{
		map models/weapons2/gauntlet/ldagauntlet_afx3.tga
		blend add
		tcMod scroll 256 0
		tcMod scale 0.01 1
	}
}

