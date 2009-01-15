/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution. 
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer. 
 *
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution. 
 *
 * - Neither the name of the <ORGANIZATION> nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software without 
 *   specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. 
 *
 */

package cubridmanager.dialog;

import org.eclipse.core.runtime.Platform;
import org.eclipse.osgi.service.datalocation.Location;
import org.eclipse.swt.SWT;
import org.eclipse.swt.browser.Browser;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Dialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;

import cubridmanager.CommonTool;
import cubridmanager.Messages;

public class HelpDialog extends Dialog {

	private Shell sShell = null; // @jve:decl-index=0:visual-constraint="41,8"
	private static boolean isAlreadyOpened = false;
	private Browser browser = null;

	public HelpDialog(Shell parent) {
		super(parent);
		// TODO Auto-generated constructor stub
	}

	public HelpDialog(Shell parent, int style) {
		super(parent, style);
		// TODO Auto-generated constructor stub
	}

	public void doModal() {
		if (isAlreadyOpened)
			return;
		else
			isAlreadyOpened = true;

		try {
			createSShell();

			// sShell.pack();
			CommonTool.centerShell(sShell);
			sShell.open();
		} catch (Exception e) {
			CommonTool.debugPrint(e);
		}

		Display display = sShell.getDisplay();
		while (!sShell.isDisposed()) {
			if (!display.readAndDispatch())
				display.sleep();
		}
	}

	/**
	 * This method initializes sShell
	 * 
	 */
	private void createSShell() {
		sShell = new Shell();
		sShell.setText(Messages.getString("TITLE.HELP"));
		sShell.setLayout(new FillLayout());
		createBrowser();
		sShell.setImage(new Image(null, cubridmanager.CubridmanagerPlugin
				.getImageDescriptor("/icons/help.png").getImageData()));
		sShell.setSize(new org.eclipse.swt.graphics.Point(1024, 768));
		sShell.addDisposeListener(new DisposeListener() {
			public void widgetDisposed(DisposeEvent e) {
				isAlreadyOpened = false;
			}
		});
	}

	/**
	 * This method initializes browser
	 * 
	 */
	private void createBrowser() {
		browser = new Browser(sShell, SWT.NONE);
		Location installLocation = Platform.getInstallLocation();
		String url = installLocation.getURL().toString().concat("../doc/index.htm");
		browser.setUrl(url);
	}
}
