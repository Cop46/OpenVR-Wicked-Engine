You need Wicked engine and OpenVR lib and header to compile this code.
For now only tested on meta quest 3.

To use this code, start the VR session like this :
EngineVrManager::getInstance()->startVrSession(wi::scene::GetScene());

And in the renderParh render after RenderPath2D::Render() call this :
EngineVrManager::getInstance()->render(dt);

You can use this code for all you want.
