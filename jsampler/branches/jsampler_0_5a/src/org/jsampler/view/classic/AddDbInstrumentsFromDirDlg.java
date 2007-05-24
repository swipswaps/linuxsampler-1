/*
 *   JSampler - a java front-end for LinuxSampler
 *
 *   Copyright (C) 2005-2007 Grigor Iliev <grigor@grigoriliev.com>
 *
 *   This file is part of JSampler.
 *
 *   JSampler is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2
 *   as published by the Free Software Foundation.
 *
 *   JSampler is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with JSampler; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *   MA  02111-1307  USA
 */

package org.jsampler.view.classic;

import java.awt.Dialog;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Insets;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;

import javax.swing.BorderFactory;
import javax.swing.Box;
import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFileChooser;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.SpinnerNumberModel;
import javax.swing.SwingUtilities;

import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;

import net.sf.juife.OkCancelDialog;
import net.sf.juife.Task;

import net.sf.juife.event.TaskEvent;
import net.sf.juife.event.TaskListener;

import org.jsampler.CC;
import org.jsampler.task.InstrumentsDb;

import static org.jsampler.view.classic.ClassicI18n.i18n;


/**
 *
 * @author Grigor Iliev
 */
public class AddDbInstrumentsFromDirDlg extends OkCancelDialog {
	private final JTextField tfSource = new JTextField();
	private final JButton btnBrowse = new JButton(Res.iconFolderOpen16);
	private final JCheckBox checkScanSubdirs =
		new JCheckBox(i18n.getLabel("AddDbInstrumentsFromDirDlg.checkScanSubdirs"));
	private final JCheckBox checkFlat =
		new JCheckBox(i18n.getLabel("AddDbInstrumentsFromDirDlg.checkFlat"));
	
	private final JTextField tfDest = new JTextField();
	private final JButton btnBrowseDb = new JButton(Res.iconFolderOpen16);
	
	/**
	 * Creates a new instance of <code>AddDbInstrumentsFromFileDlg</code>
	 */
	public
	AddDbInstrumentsFromDirDlg(Frame owner, String dbDir) {
		super(owner, i18n.getLabel("AddDbInstrumentsFromDirDlg.title"));
		
		initAddDbInstrumentsFromDirDlg(dbDir);
	}
	
	/**
	 * Creates a new instance of <code>AddDbInstrumentsFromFileDlg</code>
	 */
	public
	AddDbInstrumentsFromDirDlg(Dialog owner, String dbDir) {
		super(owner, i18n.getLabel("AddDbInstrumentsFromDirDlg.title"));
		
		initAddDbInstrumentsFromDirDlg(dbDir);
	}
	
	private void
	initAddDbInstrumentsFromDirDlg(String dbDir) {
		JPanel p = new JPanel();
		p.setLayout(new BoxLayout(p, BoxLayout.Y_AXIS));
		
		JPanel p2 = new JPanel();
		p2.setLayout(new BoxLayout(p2, BoxLayout.X_AXIS));
		p2.add(tfSource);
		p2.add(Box.createRigidArea(new Dimension(6, 0)));
		String s = i18n.getButtonLabel("AddDbInstrumentsFromDirDlg.btnBrowse");
		btnBrowse.setToolTipText(s);
		btnBrowse.setMargin(new Insets(0, 0, 0, 0));
		p2.add(btnBrowse);
		p2.setBorder(BorderFactory.createEmptyBorder(0, 3, 0, 3));
		p2.setMaximumSize(new Dimension(Short.MAX_VALUE, p2.getPreferredSize().height));
		p2.setAlignmentX(LEFT_ALIGNMENT);
		
		p.add(p2);
		
		p2 = new JPanel();
		p2.setLayout(new BoxLayout(p2, BoxLayout.X_AXIS));
		p2.add(checkScanSubdirs);
		p2.setBorder(BorderFactory.createEmptyBorder(0, 3, 0, 3));
		p2.setAlignmentX(LEFT_ALIGNMENT);
		p.add(p2);
		
		p2 = new JPanel();
		p2.setLayout(new BoxLayout(p2, BoxLayout.X_AXIS));
		p2.add(checkFlat);
		p2.setBorder(BorderFactory.createEmptyBorder(0, 15, 3, 3));
		p2.setAlignmentX(LEFT_ALIGNMENT);
		
		p.add(p2);
		
		s = i18n.getLabel("AddDbInstrumentsFromDirDlg.source");
		p.setBorder(BorderFactory.createTitledBorder(s));
		
		JPanel mainPane = new JPanel();
		mainPane.setLayout(new BoxLayout(mainPane, BoxLayout.Y_AXIS));
		
		mainPane.add(p);
		mainPane.add(Box.createRigidArea(new Dimension(0, 6)));
		
		p = new JPanel();
		p.setLayout(new BoxLayout(p, BoxLayout.Y_AXIS));
		
		p2 = new JPanel();
		p2.setLayout(new BoxLayout(p2, BoxLayout.X_AXIS));
		p2.add(tfDest);
		p2.add(Box.createRigidArea(new Dimension(6, 0)));
		s = i18n.getButtonLabel("AddDbInstrumentsFromDirDlg.btnBrowseDb");
		btnBrowseDb.setToolTipText(s);
		btnBrowseDb.setMargin(new Insets(0, 0, 0, 0));
		p2.add(btnBrowseDb);
		p2.setBorder(BorderFactory.createEmptyBorder(0, 3, 3, 3));
		p2.setMaximumSize(new Dimension(Short.MAX_VALUE, p2.getPreferredSize().height));
		p2.setAlignmentX(LEFT_ALIGNMENT);
		
		p.add(p2);
		
		s = i18n.getLabel("AddDbInstrumentsFromDirDlg.dest");
		p.setBorder(BorderFactory.createTitledBorder(s));
		
		mainPane.add(p);
		Dimension d = mainPane.getPreferredSize();
		mainPane.setPreferredSize(new Dimension(d.width > 300 ? d.width : 300, d.height));
				
		setMainPane(mainPane);
		
		setMinimumSize(this.getPreferredSize());
		setResizable(true);
		
		if(dbDir != null) tfDest.setText(dbDir);
		
		btnOk.setEnabled(false);
		checkScanSubdirs.doClick(0);
		checkFlat.doClick(0);
		
		tfSource.getDocument().addDocumentListener(getHandler());
		tfDest.getDocument().addDocumentListener(getHandler());
		
		btnBrowse.addActionListener(new ActionListener() {
			public void
			actionPerformed(ActionEvent e) { onBrowse(); }
		});
		
		checkScanSubdirs.addItemListener(getHandler());
		
		btnBrowseDb.addActionListener(new ActionListener() {
			public void
			actionPerformed(ActionEvent e) {
				DbDirectoryChooser dlg;
				dlg = new DbDirectoryChooser(AddDbInstrumentsFromDirDlg.this);
				String s = tfDest.getText();
				if(s.length() > 0) dlg.setSelectedDirectory(s);
				dlg.setVisible(true);
				if(dlg.isCancelled()) return;
				tfDest.setText(dlg.getSelectedDirectory());
			}
		});
	}
	
	private void
	onBrowse() {
		JFileChooser fc = new JFileChooser();
		fc.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
		int result = fc.showOpenDialog(this);
		if(result != JFileChooser.APPROVE_OPTION) return;
		
		tfSource.setText(fc.getSelectedFile().getPath());
	}
	
	private void
	updateState() {
		boolean b = tfSource.getText().length() != 0 && tfDest.getText().length() != 0;
		btnOk.setEnabled(b);
	}
	
	protected void
	onOk() {
		if(!btnOk.isEnabled()) return;
		
		btnOk.setEnabled(false);
		String dbDir = tfDest.getText();
		String fsDir = tfSource.getText();
		boolean recursive = checkScanSubdirs.isSelected();
		boolean flat = !checkFlat.isSelected();
		if(recursive) runTask(new InstrumentsDb.AddInstruments(dbDir, fsDir, flat));
		else runTask(new InstrumentsDb.AddInstrumentsNonrecursive(dbDir, fsDir));
		
		//new AddDbInstrumentsProgressDlg(null).setVisible(true);
	}
	
	protected void
	onCancel() { setVisible(false); }
	
	private void
	runTask(final Task<Integer> t) {
		t.addTaskListener(new TaskListener() {
			public void
			taskPerformed(TaskEvent e) {
				updateState();
				if(t.doneWithErrors()) return;
				showProgress(t.getResult());
			}
		});
		
		CC.getTaskQueue().add(t);
	}
	
	private void
	showProgress(int jobId) {
		final AddDbInstrumentsProgressDlg dlg;
		dlg = new AddDbInstrumentsProgressDlg(this, jobId);
		SwingUtilities.invokeLater(new Runnable() {
			public void
			run() { dlg.setVisible(true); }
		});
	}
	
	private final Handler eventHandler = new Handler();
	
	private Handler
	getHandler() { return eventHandler; }
	
	private class Handler implements DocumentListener, ItemListener {
		// DocumentListener
		public void
		insertUpdate(DocumentEvent e) { updateState(); }
		
		public void
		removeUpdate(DocumentEvent e) { updateState(); }
		
		public void
		changedUpdate(DocumentEvent e) { updateState(); }
		
		public void
		itemStateChanged(ItemEvent e) {
			checkFlat.setEnabled(checkScanSubdirs.isSelected());
		}
	}
}
